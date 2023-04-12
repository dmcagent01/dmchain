/*
 * dmc.cpp
 *
 *  Created on: Jun 1, 2018
 *      Author: lion
 */

#include "config.hpp"

#include "fibjs.h"
#include "object.h"
#include "include/libplatform/libplatform.h"
#include <eosio/chain/transaction_context.hpp>

namespace fibjs {
void importModule()
{
    importNativeModule();
    IMPORT_MODULE(chain)
}

int init_eos();
} // namespace fibjs

int32_t main(int32_t argc, char* argv[])
{
    class Platform : public v8::Platform {
    public:
        static v8::Platform* platform_creator()
        {
            return new Platform();
        }

        explicit Platform()
            : platform_(v8::platform::NewDefaultPlatform().release())
        {
        }

        v8::PageAllocator* GetPageAllocator() override
        {
            return platform_->GetPageAllocator();
        }

        void OnCriticalMemoryPressure() override
        {
            platform_->OnCriticalMemoryPressure();
        }

        bool OnCriticalMemoryPressure(size_t length) override
        {
            return platform_->OnCriticalMemoryPressure(length);
        }

        std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(v8::Isolate* isolate) override
        {
            return platform_->GetForegroundTaskRunner(isolate);
        }

        int NumberOfWorkerThreads() override
        {
            return platform_->NumberOfWorkerThreads();
        }

        void CallOnWorkerThread(std::unique_ptr<v8::Task> task) override
        {
            platform_->CallOnWorkerThread(std::move(task));
        }

        void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task, double delay_in_seconds) override
        {
            platform_->CallDelayedOnWorkerThread(std::move(task), delay_in_seconds);
        }

        void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) override
        {
            platform_->CallOnForegroundThread(isolate, task);
        }

        void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task, double delay_in_seconds) override
        {
            platform_->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
        }

        void CallIdleOnForegroundThread(v8::Isolate* isolate, v8::IdleTask* task) override
        {
            platform_->CallIdleOnForegroundThread(isolate, task);
        }

        bool IdleTasksEnabled(v8::Isolate* isolate) override
        {
            return platform_->IdleTasksEnabled(isolate);
        }

        double MonotonicallyIncreasingTime() override
        {
            return platform_->MonotonicallyIncreasingTime();
        }

        double CurrentClockTimeMillis() override
        {
            return platform_->CurrentClockTimeMillis();
        }

        v8::TracingController* GetTracingController() override
        {
            return platform_->GetTracingController();
        }

    private:
        v8::Platform* platform_;
    };

    fibjs::init_eos();
    fibjs::importModule();

    fibjs::start(argc, argv, fibjs::FiberProcJsEntry, Platform::platform_creator);
    fibjs::run_gui(argc, argv);

    return 0;
}
