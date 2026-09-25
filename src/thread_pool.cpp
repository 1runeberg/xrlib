/* 
 * Copyright 2024-26 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/

#include <xrlib/thread_pool.hpp>

namespace xrlib
{

	size_t CThreadPool::GetOptimalWorkerThreadCount()
	{
		const auto systemThreads = std::thread::hardware_concurrency();
		// Need minimum 5 threads (2 dedicated + 2 worker + main + system)
		assert( systemThreads >= MIN_THREAD_CAP && "System doesn't meet minimum thread requirement!" );

		// Use system threads minus 2 (for main thread and system), but ensure at least 1 worker
		size_t availableThreads = systemThreads > 2 ? systemThreads - 2 : 2;
		// Subtract 2 more for dedicated threads (render and input)
		size_t workerCount = availableThreads > 2 ? availableThreads - 2 : 2;

		return workerCount;
	}

	#ifdef XR_PLATFORM_ANDROID
		CThreadPool::CThreadPool( JavaVM *jvm )
			: CThreadPool( jvm, GetOptimalWorkerThreadCount() )
		{
		}
	#else
		CThreadPool::CThreadPool()
			: CThreadPool( GetOptimalWorkerThreadCount() )
		{
		}
	#endif

	#ifdef XR_PLATFORM_ANDROID
		CThreadPool::CThreadPool( JavaVM *jvm, size_t workerThreadCount )
			: pJvm( jvm )
		{
			assert( jvm );

			const auto systemThreads = std::thread::hardware_concurrency();
			assert( systemThreads >= MIN_THREAD_CAP && "System doesn't meet minimum thread requirement!" );

			InitializeThreads( workerThreadCount );
		}
	#else
		CThreadPool::CThreadPool( std::size_t workerThreadCount )
		{
			const auto systemThreads = std::thread::hardware_concurrency();
			assert( systemThreads >= MIN_THREAD_CAP && "System doesn't meet minimum thread requirement!" );

			InitializeThreads( workerThreadCount );
		}
	#endif
	CThreadPool::~CThreadPool() { Shutdown(); }

	void CThreadPool::WaitForThread( EThreadType type )
	{
		std::unique_lock lock( m_syncMutex );
		if ( type == EThreadType::Worker )
		{
			while ( !m_taskQueue.empty() )
			{
				m_syncCondition.wait( lock );
			}
		}
		else
		{
			auto &thread = GetDedicatedThread( type );
			while ( thread.busy )
			{
				m_syncCondition.wait( lock );
			}
		}
	}

	void CThreadPool::WaitForAll()
	{
		WaitForThread( EThreadType::Render );
		WaitForThread( EThreadType::Input );
		WaitForThread( EThreadType::Worker );
	}

	void CThreadPool::InitializeThreads( size_t workerCount )
	{
		CreateDedicatedThread( EThreadType::Render );
		CreateDedicatedThread( EThreadType::Input );

		m_running = true;
		const size_t maxThreads = std::thread::hardware_concurrency() - 2;

		InitializeThreadPool( maxThreads );

		#ifndef XR_PLATFORM_ANDROID
		m_scalingThread = std::thread( &CThreadPool::ScaleWorkerThreads, this );
		#endif

	}
	void CThreadPool::SetThreadPriority( std::thread &thread, EThreadPriority priority )
	{
	#ifdef XR_PLATFORM_WINDOWS
		int winPriority;
		switch ( priority )
		{
			case EThreadPriority::Low:
				winPriority = THREAD_PRIORITY_BELOW_NORMAL;
				break;
			case EThreadPriority::Normal:
				winPriority = THREAD_PRIORITY_NORMAL;
				break;
			case EThreadPriority::High:
				winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
				break;
			case EThreadPriority::RealTime:
				winPriority = THREAD_PRIORITY_TIME_CRITICAL;
				break;
			default:
				winPriority = THREAD_PRIORITY_NORMAL;
		}
		::SetThreadPriority( thread.native_handle(), winPriority );
	#elif defined( XR_PLATFORM_ANDROID ) || defined( XR_PLATFORM_LINUX )
		int policy;
		struct sched_param param;
		pthread_getschedparam( thread.native_handle(), &policy, &param );

		switch ( priority )
		{
			case EThreadPriority::Low:
				param.sched_priority = sched_get_priority_min( SCHED_OTHER );
				break;
			case EThreadPriority::Normal:
				param.sched_priority = ( sched_get_priority_max( SCHED_OTHER ) + sched_get_priority_min( SCHED_OTHER ) ) / 2;
				break;
			case EThreadPriority::High:
				param.sched_priority = sched_get_priority_max( SCHED_OTHER );
				break;
			case EThreadPriority::RealTime:
				policy = SCHED_RR;
				param.sched_priority = sched_get_priority_max( SCHED_RR );
				break;
		}
		pthread_setschedparam( thread.native_handle(), policy, &param );
	#elif defined( XR_PLATFORM_MACOS )
		thread_port_t mach_thread = pthread_mach_thread_np( thread.native_handle() );
		thread_precedence_policy_data_t precedence;

		switch ( priority )
		{
			case EThreadPriority::Low:
				precedence.importance = -1;
				break;
			case EThreadPriority::Normal:
				precedence.importance = 0;
				break;
			case EThreadPriority::High:
				precedence.importance = 1;
				break;
			case EThreadPriority::RealTime:
				precedence.importance = 2;
				break;
		}

		thread_policy_set( mach_thread, THREAD_PRECEDENCE_POLICY, (thread_policy_t) &precedence, THREAD_PRECEDENCE_POLICY_COUNT );
	#endif
	}

	void CThreadPool::SetThreadAffinity( std::thread &thread, uint32_t cpuCore )
	{
		if ( cpuCore == ( std::numeric_limits< uint32_t >::max )() )
			return;

	#ifdef XR_PLATFORM_WINDOWS
		DWORD_PTR mask = 1ULL << cpuCore;
		SetThreadAffinityMask( thread.native_handle(), mask );
	#elif defined( XR_PLATFORM_ANDROID ) || defined( XR_PLATFORM_LINUX )
		cpu_set_t cpuset;
		CPU_ZERO( &cpuset );
		CPU_SET( cpuCore, &cpuset );
		sched_setaffinity( thread.native_handle(), sizeof( cpu_set_t ), &cpuset );
	#elif defined( XR_PLATFORM_MACOS )
			// macOS doesn't support thread affinity directly
	#endif
	}

	void CThreadPool::CreateDedicatedThread( EThreadType type )
	{
		auto &dedicated = m_dedicatedThreads[ type ];
		EThreadType threadType = type; // Create a copy for the lambda

		dedicated.thread = std::thread(
			[ this, &dedicated, threadType ]()
			{
				#ifdef XR_PLATFORM_ANDROID
					ThreadAttacher attacher( pJvm );
				#endif

				// Set default priorities
				if ( threadType == EThreadType::Render )
				{
					this->SetThreadPriority( dedicated.thread, EThreadPriority::High );
				}
				else if ( threadType == EThreadType::Input )
				{
					this->SetThreadPriority( dedicated.thread, EThreadPriority::Normal );
				}

				while ( !dedicated.stop )
				{
					std::function< void() > task;
					{
						std::unique_lock lock( dedicated.mutex );
						dedicated.condition.wait( lock, [ &dedicated ]() { return !dedicated.taskQueue.empty() || dedicated.stop; } );
						if ( dedicated.stop && dedicated.taskQueue.empty() )
							return;
						task = std::move( dedicated.taskQueue.front() );
						dedicated.taskQueue.pop();
					}
					dedicated.busy = true;
					task();
					dedicated.busy = false;
					m_syncCondition.notify_all();
				}
			} );
	}

	SDedicatedThread &CThreadPool::GetDedicatedThread( EThreadType type ) { return m_dedicatedThreads[ type ]; }

	void CThreadPool::ScaleWorkerThreads() 
	{
		const size_t maxThreads = std::thread::hardware_concurrency() - 3; // Account for dedicated threads and system thread

		while ( m_running )
		{
			std::this_thread::sleep_for( std::chrono::milliseconds( SCALE_CHECK_INTERVAL ) );

			double utilizationRatio = static_cast< double >( m_activeWorkers ) / m_currentWorkers;

			if ( utilizationRatio >= SCALE_UP_THRESHOLD && m_currentWorkers < maxThreads )
			{
				AddWorkerThread();
			}
			else if ( utilizationRatio <= SCALE_DOWN_THRESHOLD && m_currentWorkers > MIN_WORKER_THREADS )
			{
				RemoveWorkerThread();
			}
		}
	}

	void CThreadPool::AddWorkerThread() 
	{
		std::unique_lock lock( m_taskMutex );
		m_workerThreads.emplace_back(
			[ this ]()
			{
				#ifdef XR_PLATFORM_ANDROID
				ThreadAttacher attacher( pJvm );
				#endif

				while ( m_running )
				{
					std::function< void() > task;
					{
						std::unique_lock lock( m_taskMutex );
						m_taskCondition.wait( lock, [ this ]() { return !m_taskQueue.empty() || !m_running; } );

						if ( !m_running && m_taskQueue.empty() )
							return;

						task = std::move( m_taskQueue.front() );
						m_taskQueue.pop();
					}

					m_activeWorkers++;
					task();
					m_activeWorkers--;
					m_syncCondition.notify_all();
				}
			} );
		m_currentWorkers++;
	}

	void CThreadPool::RemoveWorkerThread() 
	{
		std::unique_lock lock( m_taskMutex );
		if ( !m_workerThreads.empty() )
		{
			m_workerThreads.back().detach();
			m_workerThreads.pop_back();
			m_currentWorkers--;
		}
	}

	void CThreadPool::InitializeThreadPool( size_t maxThreads ) 
	{
		std::lock_guard lock( m_threadPoolMutex );

		for ( size_t i = 0; i < maxThreads; ++i )
		{
			auto worker = std::make_unique< SWorkerThread >();
			worker->thread = std::thread(
				[ this, i ]()
				{
					#ifdef XR_PLATFORM_ANDROID
					ThreadAttacher attacher( pJvm );
					#endif

					while ( m_running )
					{
						{
							std::unique_lock lock( m_threadPoolMutex );
							m_availableThreads.push( i );
						}

						std::function< void() > task;
						{
							std::unique_lock lock( m_taskMutex );
							m_taskCondition.wait(
								lock,
								[ this, i ]()
								{
									auto &worker = m_threadPool[ i ];
									return ( !m_taskQueue.empty() && !worker->parked ) || !m_running;
								} );

							if ( !m_running )
								return;

							task = std::move( m_taskQueue.front() );
							m_taskQueue.pop();
						}

						m_threadPool[ i ]->active = true;
						m_activeWorkers++;

						task();

						m_activeWorkers--;
						m_threadPool[ i ]->active = false;
						m_syncCondition.notify_all();
					}
				} );
			m_threadPool.push_back( std::move( worker ) );
		}
	}

	size_t CThreadPool::GetOrCreateThread() 
	{ 
		std::lock_guard lock( m_threadPoolMutex );
		if ( m_availableThreads.empty() )
		{
			return SIZE_MAX;
		}
		size_t idx = m_availableThreads.front();
		m_availableThreads.pop();
		return idx;
	}

	void CThreadPool::ParkThread( size_t threadIdx ) 
	{
		if ( threadIdx >= m_threadPool.size() )
			return;

		std::lock_guard lock( m_threadPoolMutex );
		m_threadPool[ threadIdx ]->parked = true;

	}

	void CThreadPool::Shutdown()
	{
		{
			std::unique_lock lock( m_taskMutex );
			m_running = false;
		}
		m_taskCondition.notify_all();
		for ( auto &thread : m_workerThreads )
		{
			if ( thread.joinable() )
			{
				thread.join();
			}
		}

		for ( auto &[ type, dedicated ] : m_dedicatedThreads )
		{
			dedicated.stop = true;
			dedicated.condition.notify_one();
			if ( dedicated.thread.joinable() )
			{
				dedicated.thread.join();
			}
		}
	}

} // namespace xrlib