//
//  main.c
//  ash
//
//  Created by David Semke on 9/30/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include "avmshell.h"
#include "PosixPartialPlatform.h"

namespace avmshell
{
    class MacPlatform : public PosixPartialPlatform
    {
    public:
        MacPlatform(void* stackbase) : stackbase(stackbase) {}
        virtual ~MacPlatform() {}
		
        virtual void setTimer(int seconds, AvmTimerCallback callback, void* callbackData);
        virtual uintptr_t getMainThreadStackLimit();
		
    private:
        void* stackbase;
		
    };
	
    uintptr_t MacPlatform::getMainThreadStackLimit()
    {
        struct rlimit r;
        size_t stackheight = avmshell::kStackSizeFallbackValue;
        // https://bugzilla.mozilla.org/show_bug.cgi?id=504976: setting the height to kStackSizeFallbackValue
        // is not ideal if the stack is meant to be unlimited but is an OK workaround for the time being.
        if (getrlimit(RLIMIT_STACK, &r) == 0 && r.rlim_cur != RLIM_INFINITY)
            stackheight = size_t(r.rlim_cur);
        return uintptr_t(stackbase) - stackheight + avmshell::kStackMargin;
    }
	
    AvmTimerCallback pCallbackFunc = 0;
    void* pCallbackData = 0;
	
    void MacPlatform::setTimer(int seconds, AvmTimerCallback callback, void* callbackData)
    {
        extern void alarmProc(int);
		
        pCallbackFunc = callback;
        pCallbackData = callbackData;
		
        signal(SIGALRM, alarmProc);
        alarm(seconds);
		
    }
	
    void alarmProc(int /*signum*/)
    {
        pCallbackFunc(pCallbackData);
    }
}

avmshell::MacPlatform *platform_handle;

avmshell::Platform* avmshell::Platform::GetInstance()
{
    AvmAssert(platform_handle != NULL);
    return platform_handle;
}

int main (int argc, char **argv)
{
    char *dummy; /* ?? */
    avmshell::MacPlatform platform_instance(&dummy);
    platform_handle = &platform_instance;
	
	int code = avmshell::Shell::run(argc, argv);
	return code;
}