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


#pragma once
#define MIN_THREAD_CAP 6

// Platform detection
#if defined( _WIN32 ) || defined( _WIN64 )
	#define XR_PLATFORM_WINDOWS
#elif defined( __APPLE__ )
	#define XR_PLATFORM_MACOS
#elif defined( __ANDROID__ )
	#define XR_PLATFORM_ANDROID
#elif defined( __linux__ )
	#define XR_PLATFORM_LINUX
#else
	#error "Unsupported platform"
#endif

// Platform-specific headers
#ifdef XR_PLATFORM_WINDOWS
	#define WIN32_LEAN_AND_MEAN

	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#include <windows.h>
#elif defined( XR_PLATFORM_ANDROID ) || defined( XR_PLATFORM_LINUX )
	#include <pthread.h>
	#include <sched.h>
	#ifdef XR_PLATFORM_ANDROID
		#include <jni.h>
	#endif
#elif defined( XR_PLATFORM_MACOS )
	#include <mach/mach.h>
	#include <mach/thread_policy.h>
	#include <pthread.h>
#endif

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace xrlib
{
#ifdef XR_PLATFORM_ANDROID
	class ThreadAttacher
	{
	  public:
		ThreadAttacher( JavaVM *jvm )
			: m_jvm( jvm )
		{
			if ( m_jvm )
			{
				JNIEnv *env;
				jint result = m_jvm->GetEnv( (void **) &env, JNI_VERSION_1_4 );
				if ( result == JNI_EDETACHED )
				{
					JavaVMAttachArgs args;
					args.version = JNI_VERSION_1_4;
					args.name = "NativeThread";
					args.group = nullptr;
					result = m_jvm->AttachCurrentThread( &env, &args );
					m_attached = ( result == JNI_OK );
				}
			}
		}

		~ThreadAttacher()
		{
			if ( m_attached && m_jvm )
			{
				m_jvm->DetachCurrentThread();
			}
		}

		ThreadAttacher( const ThreadAttacher & ) = delete;
		ThreadAttacher &operator=( const ThreadAttacher & ) = delete;

	  private:
		JavaVM *m_jvm;
		bool m_attached = false;
	};
#endif

	enum class EThreadType
	{
		Render,
		Input,
		Worker
	};

	enum class EThreadPriority
	{
		Low,
		Normal,
		High,
		RealTime
	};

	struct SThreadConfig
	{
		EThreadPriority priority = EThreadPriority::Normal;
		uint32_t cpuCore = ( std::numeric_limits< uint32_t >::max )();
	};

	struct SDedicatedThread
	{
		std::thread thread;
		std::queue< std::function< void() > > taskQueue;
		std::mutex mutex;
		std::condition_variable condition;
		std::atomic< bool > busy { false };
		std::atomic< bool > stop { false };
	};

	struct SWorkerThread
	{
		std::thread thread;
		std::atomic< bool > parked { false };
		std::atomic< bool > active { false };
	};

	class CThreadPool
	{
	  public:
		static constexpr size_t MIN_WORKER_THREADS = 2;

		#ifdef XR_PLATFORM_ANDROID
			CThreadPool( JavaVM *jvm );							  // Default constructor uses optimal count
			CThreadPool( JavaVM *jvm, size_t workerThreadCount ); // Parameterized constructor for specific count
		#else
			CThreadPool();									  // Default constructor uses optimal count
			explicit CThreadPool( size_t workerThreadCount ); // Parameterized constructor for specific count
		#endif

		~CThreadPool();

		// Delete copy constructor and assignment operator
		CThreadPool( const CThreadPool & ) = delete;
		CThreadPool &operator=( const CThreadPool & ) = delete;

		static size_t GetOptimalWorkerThreadCount();

		template < typename F, typename... Args > auto SubmitTask( F &&f, Args &&...args )
		{
			using ReturnType = std::invoke_result_t< F, Args... >;
			auto pTask = std::make_shared< std::packaged_task< ReturnType() > >( std::bind( std::forward< F >( f ), std::forward< Args >( args )... ) );
			auto future = pTask->get_future();

			{
				std::unique_lock lock( m_taskMutex );
				m_taskQueue.emplace( [ pTask ]() { ( *pTask )(); } );
			}
			m_taskCondition.notify_one();
			return future;
		}

		template < typename F, typename... Args > auto SubmitRenderTask( F &&f, Args &&...args ) { return SubmitDedicatedTask( EThreadType::Render, std::forward< F >( f ), std::forward< Args >( args )... ); }

		template < typename F, typename... Args > auto SubmitInputTask( F &&f, Args &&...args ) { return SubmitDedicatedTask( EThreadType::Input, std::forward< F >( f ), std::forward< Args >( args )... ); }

		void WaitForThread( EThreadType type );
		void WaitForAll();

		#ifdef XR_PLATFORM_ANDROID
			JavaVM *pJvm = nullptr;
		#endif

	  private:
		void InitializeThreads( size_t workerCount );
		void CreateDedicatedThread( EThreadType type );
		void Shutdown();
		void SetThreadPriority( std::thread &thread, EThreadPriority priority );
		void SetThreadAffinity( std::thread &thread, uint32_t cpuCore );
		SDedicatedThread &GetDedicatedThread( EThreadType type );

		template < typename F, typename... Args > auto SubmitDedicatedTask( EThreadType type, F &&f, Args &&...args )
		{
			using ReturnType = std::invoke_result_t< F, Args... >;
			auto pTask = std::make_shared< std::packaged_task< ReturnType() > >( std::bind( std::forward< F >( f ), std::forward< Args >( args )... ) );
			auto future = pTask->get_future();

			auto &dedicated = GetDedicatedThread( type );
			{
				std::unique_lock lock( dedicated.mutex );
				dedicated.taskQueue.emplace( [ pTask ]() { ( *pTask )(); } );
			}
			dedicated.condition.notify_one();
			return future;
		}

		std::atomic< bool > m_running { false };
		std::vector< std::thread > m_workerThreads;
		std::queue< std::function< void() > > m_taskQueue;
		std::mutex m_taskMutex;
		std::condition_variable m_taskCondition;

		std::map< EThreadType, SDedicatedThread > m_dedicatedThreads;
		std::mutex m_syncMutex;
		std::condition_variable m_syncCondition;

		std::atomic< size_t > m_activeWorkers { 0 };
		std::atomic< size_t > m_currentWorkers { 0 };

		static constexpr double SCALE_UP_THRESHOLD = 0.75;	 // Scale up when 75% of threads are active
		static constexpr double SCALE_DOWN_THRESHOLD = 0.25; // Scale down when 25% of threads are active
		static constexpr size_t SCALE_CHECK_INTERVAL = 1000; // Check scaling every 1000ms

		std::vector< std::unique_ptr< SWorkerThread > > m_threadPool;
		std::queue< size_t > m_availableThreads;
		std::mutex m_threadPoolMutex;
    
		void ScaleWorkerThreads();
		void AddWorkerThread();
		void RemoveWorkerThread();

		void InitializeThreadPool( size_t maxThreads );
		size_t GetOrCreateThread();
		void ParkThread( size_t threadIdx );

		std::thread m_scalingThread;
	};

} // namespace xrlib