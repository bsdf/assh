/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "avmshell.h"
#ifdef VMCFG_NANOJIT
#include "../nanojit/nanojit.h"
#endif
#include <float.h>

#include "extensions-tracers.hh"
#include "avmshell-tracers.hh"

#define LOGGING(x)

namespace avmshell
{
    ShellSettings::ShellSettings()
    : ShellCoreSettings()
    , programFilename(NULL)
    , filenames(NULL)
    , numfiles(-1)
    , do_selftest(false)
    , do_repl(true)
    , do_log(false)
    , do_projector(false)
    , numthreads(1)
    , numworkers(1)
    , repeats(1)
    , stackSize(0)
    {
    }
    
    ShellCoreImpl::ShellCoreImpl(MMgc::GC* gc, ShellSettings& settings, bool mainthread)
    : ShellCore(gc, settings.apiVersionSeries)
    , settings(settings)
    , mainthread(mainthread)
    {
    }
    
    /* virtual */
    void ShellCoreImpl::setStackLimit()
    {
        uintptr_t minstack;
        if (settings.stackSize != 0)
        {
            // Here we really depend on being called fairly high up on
            // the thread's stack, because we don't know where the highest
            // stack address is.
            minstack = uintptr_t(&minstack) - settings.stackSize + avmshell::kStackMargin;
        }
        else
        {

#ifdef VMCFG_WORKERTHREADS
            if (mainthread)
                //minstack = Platform::GetInstance()->getMainThreadStackLimit();
                minstack = 10;
            else {
                // Here we really depend on being called fairly high up on
                // the thread's stack, because we don't know where the highest
                // stack address is.
                // In order to be platform-independent, we can no
                // longer rely on getting the default stack height
                // from a pthread call. Instead, we get it from the
                // VMPI_threadAttrDefaultStackSize() call.
                size_t stackheight = VMPI_threadAttrDefaultStackSize();
                minstack = uintptr_t(&stackheight) - stackheight + avmshell::kStackMargin;
            }
#else
            minstack = Platform::GetInstance()->getMainThreadStackLimit();
#endif
        }
        
        // call the non-virtual setter on AvmCore
        AvmCore::setStackLimit(minstack);
    }

    
    // open logfile based on a filename
    /* static */
    void Shell::initializeLogging(const char* basename)
    {
        const char* lastDot = VMPI_strrchr(basename, '.');
        if(lastDot)
        {
            //filename could contain '/' or '\' as their separator, look for both
            const char* lastPathSep1 = VMPI_strrchr(basename, '/');
            const char* lastPathSep2 = VMPI_strrchr(basename, '\\');
            if(lastPathSep1 < lastPathSep2) //determine the right-most
                lastPathSep1 = lastPathSep2;
            
            //if dot is before the separator, the filename does not have an extension
            if(lastDot < lastPathSep1)
                lastDot = NULL;
        }
        
        //if no extension then take the entire filename or
        size_t logFilenameLen = (lastDot == NULL) ? VMPI_strlen(basename) : (lastDot - basename);
        
        char* logFilename = new char[logFilenameLen + 5];  // 5 bytes for ".log" + null char
        VMPI_strncpy(logFilename,basename,logFilenameLen);
        VMPI_strcpy(logFilename+logFilenameLen,".log");
        
        Platform::GetInstance()->initializeLogging(logFilename);
        
        delete [] logFilename;
    }
    
    /* static */
    void Shell::parseCommandLine(int argc, char* argv[], ShellSettings& settings)
    {
        bool print_version = false;
        
        // options filenames -- args
        
        settings.programFilename = argv[0];     // How portable / reliable is this?
        for (int i=1; i < argc ; i++) {
            const char * const arg = argv[i];
            bool mmgcSaysArgIsWrong = false;
            
            if (arg[0] == '-')
            {
                if (arg[1] == '-' && arg[2] == 0) {
                    if (settings.filenames == NULL)
                        settings.filenames = &argv[i];
                    settings.numfiles = int(&argv[i] - settings.filenames);
                    i++;
                    settings.arguments = &argv[i];
                    settings.numargs = argc - i;
                    break;
                }
                
                if (arg[1] == 'D') {
                    if (!VMPI_strcmp(arg+2, "timeout")) {
                        settings.interrupts = true;
                    }
                    else if (!VMPI_strcmp(arg+2, "version")) {
                        print_version = true;
                    }
                    else if (!VMPI_strcmp(arg+2, "nodebugger")) {
                        // allow this option even in non-DEBUGGER builds to make test scripts simpler
                        settings.nodebugger = true;
                    }
#if defined(AVMPLUS_IA32) && defined(VMCFG_NANOJIT)
                    else if (!VMPI_strcmp(arg+2, "nosse")) {
                        settings.njconfig.i386_sse2 = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "fixedesp")) {
                        settings.njconfig.i386_fixed_esp = true;
                    }
#endif /* AVMPLUS_IA32 */
#if defined(AVMPLUS_ARM) && defined(VMCFG_NANOJIT)
                    else if (!VMPI_strcmp(arg+2, "arm_arch") && i+1 < argc ) {
                        settings.njconfig.arm_arch = (uint8_t)VMPI_strtol(argv[++i], 0, 10);
                    }
                    else if (!VMPI_strcmp(arg+2, "arm_vfp")) {
                        settings.njconfig.arm_vfp = true;
                    }
#endif /* AVMPLUS_IA32 */
#ifdef VMCFG_VERIFYALL
                    else if (!VMPI_strcmp(arg+2, "verifyall")) {
                        settings.verifyall = true;
                        settings.verifyquiet = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "verifyonly")) {
                        settings.verifyall = true;
                        settings.verifyonly = true;
                        settings.verifyquiet = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "verifyquiet")) {
                        settings.verifyall = true;
                        settings.verifyonly = true;
                        settings.verifyquiet = true;
                    }
#endif /* VMCFG_VERIFYALL */
                    else if (!VMPI_strcmp(arg+2, "greedy")) {
                        settings.greedy = true;
                    }
                    else if (!VMPI_strcmp(arg+2, "nogc")) {
                        settings.nogc = true;
                    }
                    else if (!VMPI_strcmp(arg+2, "noincgc")) {
                        settings.incremental = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "drcvalidation")) {
                        settings.drcValidation = true;
                    }
#ifdef VMCFG_SELECTABLE_EXACT_TRACING
                    else if (!VMPI_strcmp(arg+2, "noexactgc")) {
                        settings.exactgc = false;
                    }
#endif
                    else if (!VMPI_strcmp(arg+2, "nodrc")) {
                        settings.drc = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "nofixedcheck")) {
                        settings.fixedcheck = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "gcthreshold") && i+1 < argc) {
                        settings.gcthreshold = VMPI_strtol(argv[++i], 0, 10);
                    }
#if defined(DEBUGGER) && !defined(VMCFG_DEBUGGER_STUB)
                    else if (!VMPI_strcmp(arg+2, "astrace") && i+1 < argc ) {
                        settings.astrace_console = VMPI_strtol(argv[++i], 0, 10);
                    }
                    else if (!VMPI_strcmp(arg+2, "language") && i+1 < argc ) {
                        settings.langID=-1;
                        for (int j=0;j<kLanguages;j++) {
                            if (!VMPI_strcmp(argv[i+1],languageNames[j].str)) {
                                settings.langID=j;
                                break;
                            }
                        }
                        if (settings.langID==-1) {
                            settings.langID = VMPI_atoi(argv[i+1]);
                        }
                        i++;
                    }
#endif /* defined(DEBUGGER) && !defined(VMCFG_DEBUGGER_STUB) */
#ifdef VMCFG_SELFTEST
                    else if (!VMPI_strncmp(arg+2, "selftest", 8)) {
                        settings.do_selftest = true;
                        if (arg[10] == '=') {
                            VMPI_strncpy(settings.st_mem, arg+11, sizeof(settings.st_mem));
                            settings.st_mem[sizeof(settings.st_mem)-1] = 0;
                            char *p = settings.st_mem;
                            settings.st_component = p;
                            while (*p && *p != ',')
                                p++;
                            if (*p == ',')
                                *p++ = 0;
                            settings.st_category = p;
                            while (*p && *p != ',')
                                p++;
                            if (*p == ',')
                                *p++ = 0;
                            settings.st_name = p;
                            if (*settings.st_component == 0)
                                settings.st_component = NULL;
                            if (*settings.st_category == 0)
                                settings.st_category = NULL;
                            if (*settings.st_name == 0)
                                settings.st_name = NULL;
                        }
                    }
#endif /* VMCFG_SELFTEST */
#ifdef AVMPLUS_VERBOSE
                    else if (!VMPI_strncmp(arg+2, "verbose-only", 12)) {
                        settings.verboseOnlyArg = argv[++i];  // capture the string process it later
                    }
                    else if (!VMPI_strncmp(arg+2, "verbose", 7)) {
                        settings.do_verbose = avmplus::AvmCore::DEFAULT_VERBOSE_ON; // all 'on' by default
                        if (arg[9] == '=') {
                            char* badFlag;
                            settings.do_verbose = avmplus::AvmCore::parseVerboseFlags(&arg[10], badFlag);
                            if (badFlag) {
                                avmplus::AvmLog("Unknown verbose flag while parsing '%s'\n", badFlag);
                                usage();
                            }
                        }
                    }
#endif /* AVMPLUS_VERBOSE */
#ifdef VMCFG_NANOJIT
                    else if (!VMPI_strcmp(arg+2, "nocse")) {
                        settings.njconfig.cseopt = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "noinline")) {
                        settings.jitconfig.opt_inline = false;
                    }
                    else if (!VMPI_strcmp(arg+2, "jitordie")) {
                        settings.runmode = avmplus::RM_jit_all;
                        settings.jitordie = true;
                    }
#endif /* VMCFG_NANOJIT */
                    else if (!VMPI_strcmp(arg+2, "interp")) {
                        settings.runmode = avmplus::RM_interp_all;
                    }
                    else {
                        avmplus::AvmLog("Unrecognized option %s\n", arg);
                        usage();
                    }
                }
                else if (!VMPI_strcmp(arg, "-cache_bindings") && i+1 < argc) {
                    settings.cacheSizes.bindings = (uint16_t)VMPI_strtol(argv[++i], 0, 10);
                }
                else if (!VMPI_strcmp(arg, "-cache_metadata") && i+1 < argc) {
                    settings.cacheSizes.metadata = (uint16_t)VMPI_strtol(argv[++i], 0, 10);
                }
                else if (!VMPI_strcmp(arg, "-cache_methods") && i+1 < argc ) {
                    settings.cacheSizes.methods = (uint16_t)VMPI_strtol(argv[++i], 0, 10);
                }
                else if (!VMPI_strcmp(arg, "-swfHasAS3")) {
                    settings.do_testSWFHasAS3 = true;
                }
#ifdef VMCFG_NANOJIT
                else if (!VMPI_strcmp(arg, "-jitharden")) {
                    settings.njconfig.harden_nop_insertion = true;
                    settings.njconfig.harden_function_alignment = true;
                }
                else if (!VMPI_strcmp(arg, "-Ojit")) {
                    settings.runmode = avmplus::RM_jit_all;
                }
#ifdef VMCFG_COMPILEPOLICY
                else if (!VMPI_strcmp(arg, "-policy")) {
                    settings.policyRulesArg = argv[++i];  // capture the string process it later
                }
#endif /* VMCFG_COMPILEPOLICY */
#ifdef VMCFG_OSR
                else if (!VMPI_strncmp(arg, "-osr=", 5)) {
                    // parse the commandline threshold
                    int32_t threshold;
                    if (VMPI_sscanf(arg + 5, "%d", &threshold) != 1 ||
                        threshold < 0) {
                        avmplus::AvmLog("Bad value to -osr: %s\n", arg + 5);
                        usage();
                    }
                    settings.osr_threshold = threshold;
                }
#endif /* VMCFG_OSR */
#endif /* VMCFG_NANOJIT */
                else if (MMgc::GCHeap::GetGCHeap()->Config().IsGCOptionWithParam(arg) && i+1 < argc ) {
                    const char *val = argv[++i];
                    if (MMgc::GCHeap::GetGCHeap()->Config().ParseAndApplyOption(arg, mmgcSaysArgIsWrong, val)) {
                        if (mmgcSaysArgIsWrong) {
                            avmplus::AvmLog("Invalid GC option: %s %s\n", arg, val);
                            usage();
                        }
                        // otherwise, option was implicitly applied during the
                        // ParseAndApplyOption invocation in the test above.
                    } else {
                        AvmAssertMsg(false, "IsGCOptionWithParam inconsistent with ParseAndApplyOption.");
                    }
                }
                else if (MMgc::GCHeap::GetGCHeap()->Config().ParseAndApplyOption(arg, mmgcSaysArgIsWrong)) {
                    if (mmgcSaysArgIsWrong) {
                        avmplus::AvmLog("Invalid GC option: %s\n", arg);
                        usage();
                    }
                }
                else if (!VMPI_strcmp(arg, "-stack") && i+1 < argc ) {
                    unsigned stack;
                    int nchar;
                    const char* val = argv[++i];
                    if (VMPI_sscanf(val, "%u%n", &stack, &nchar) == 1 && size_t(nchar) == VMPI_strlen(val) && stack > avmshell::kStackMargin) {
                        settings.stackSize = uint32_t(stack);
                    }
                    else
                    {
                        avmplus::AvmLog("Bad argument to -stack\n");
                        usage();
                    }
                }
#ifdef MMGC_MARKSTACK_ALLOWANCE
                else if (!VMPI_strcmp(arg, "-gcstack") && i+1 < argc ) {
                    int stack;
                    int nchar;
                    const char* val = argv[++i];
                    if (VMPI_sscanf(val, "%d%n", &stack, &nchar) == 1 && size_t(nchar) == VMPI_strlen(val) && stack >= 0) {
                        settings.markstackAllowance = int32_t(stack);
                    }
                    else
                    {
                        avmplus::AvmLog("Bad argument to -gcstack\n");
                        usage();
                    }
                }
#endif
                else if (!VMPI_strcmp(arg, "-log")) {
                    settings.do_log = true;
                }
#ifdef VMCFG_EVAL
                else if (!VMPI_strcmp(arg, "-repl")) {
                    settings.do_repl = true;
                }
#endif /* VMCFG_EVAL */
#ifdef VMCFG_WORKERTHREADS
                else if (!VMPI_strcmp(arg, "-workers") && i+1 < argc ) {
                    const char *val = argv[++i];
                    int nchar;
                    if (val == NULL)
                        val = "";
                    if (VMPI_sscanf(val, "%d,%d,%d%n", &settings.numworkers, &settings.numthreads, &settings.repeats, &nchar) != 3)
                        if (VMPI_sscanf(val, "%d,%d%n", &settings.numworkers, &settings.numthreads, &nchar) != 2) {
                            avmplus::AvmLog("Bad value to -workers: %s\n", val);
                            usage();
                        }
                    if (settings.numthreads < 1 ||
                        settings.numworkers < settings.numthreads ||
                        settings.repeats < 1 ||
                        size_t(nchar) != VMPI_strlen(val)) {
                        avmplus::AvmLog("Bad value to -workers: %s\n", val);
                        usage();
                    }
                }
#endif // VMCFG_WORKERTHREADS
#ifdef AVMPLUS_WIN32
                else if (!VMPI_strcmp(arg, "-error")) {
                    show_error = true;
#ifndef UNDER_CE
                    SetErrorMode(0);  // set to default
#endif
                }
#endif // AVMPLUS_WIN32
#ifdef AVMPLUS_WITH_JNI
                else if (!VMPI_strcmp(arg, "-jargs")) {
                    // all the following args until the semi colon is for java.
                    //@todo fix up this hard limit
                    bool first = true;
                    Java::startup_options = new char[256];
                    VMPI_memset(Java::startup_options, 0, 256);
                    
                    for(i++; i<argc; i++)
                    {
                        if (*argv[i] == ';')
                            break;
                        if (!first) VMPI_strcat(Java::startup_options, " ");
                        VMPI_strcat(Java::startup_options, argv[i]);
                        first = false;
                    }
                    AvmAssert(VMPI_strlen(Java::startup_options) < 256);
                }
#endif /* AVMPLUS_WITH_JNI */
#ifdef DEBUGGER
                else if (!VMPI_strcmp(arg, "-d")) {
                    settings.enter_debugger_on_launch = true;
                }
#endif /* DEBUGGER */
                else if (!VMPI_strcmp(arg, "-api") && i+1 < argc) {
                    if (!avmplus::AvmCore::parseApiVersion(argv[i+1], settings.apiVersion, settings.apiVersionSeries))
                    {
                        avmplus::AvmLog("Unknown api version'%s'\n", argv[i+1]);
                        usage();
                    }
                    i++;
                }
                else if (VMPI_strcmp(arg, "-swfversion") == 0 && i+1 < argc) {
                    int j = avmplus::BugCompatibility::VersionCount;
                    unsigned swfVersion;
                    int nchar;
                    const char* val = argv[++i];
                    if (VMPI_sscanf(val, "%u%n", &swfVersion, &nchar) == 1 && size_t(nchar) == VMPI_strlen(val))
                    {
                        for (j = 0; j < avmplus::BugCompatibility::VersionCount; ++j)
                        {
                            if (avmplus::BugCompatibility::kNames[j] == swfVersion)
                            {
                                settings.swfVersion = (avmplus::BugCompatibility::Version)j;
                                break;
                            }
                        }
                    }
                    if (j == avmplus::BugCompatibility::VersionCount) {
                        avmplus::AvmLog("Unrecognized -swfversion version %s\n", val);
                        usage();
                    }
                }
                else {
                    // Unrecognized command line option
                    avmplus::AvmLog("Unrecognized option %s\n", arg);
                    usage();
                }
            }
            else {
                if (settings.filenames == NULL)
                    settings.filenames = &argv[i];
            }
        }
        
        if (settings.filenames == NULL)
            settings.filenames = &argv[argc];
        
        if (settings.numfiles == -1)
            settings.numfiles = int(&argv[argc] - settings.filenames);
        
        if (settings.arguments == NULL) {
            settings.arguments = &argv[argc];
            settings.numargs = 0;
        }
        
        AvmAssert(settings.filenames != NULL && settings.numfiles != -1);
        AvmAssert(settings.arguments != NULL && settings.numargs != -1);
        
        if (print_version)
        {
            avmplus::AvmLog("shell " AVMPLUS_VERSION_USER " " AVMPLUS_BIN_TYPE );
            if (RUNNING_ON_VALGRIND)
                avmplus::AvmLog("-valgrind");
            avmplus::AvmLog(" build " AVMPLUS_BUILD_CODE "\n");
#ifdef AVMPLUS_DESC_STRING
            if (VMPI_strcmp(AVMPLUS_DESC_STRING, ""))
            {
                avmplus::AvmLog("Description: " AVMPLUS_DESC_STRING "\n");
            }
#endif
            avmplus::AvmLog("features %s\n", avmfeatures);
            Platform::GetInstance()->exit(1);
        }
        
        // Vetting the options
        
#ifdef AVMSHELL_PROJECTOR_SUPPORT
        if (settings.programFilename != NULL && ShellCore::isValidProjectorFile(settings.programFilename)) {
            if (settings.do_selftest || settings.do_repl || settings.numfiles > 0) {
                avmplus::AvmLog("Projector files can't be used with -repl, -Dselftest, or program file arguments.\n");
                usage();
            }
            if (settings.numthreads > 1 || settings.numworkers > 1) {
                avmplus::AvmLog("A projector requires exactly one worker on one thread.\n");
                usage();
            }
            settings.do_projector = 1;
            return;
        }
#endif
        
#ifndef VMCFG_AOT
        if (settings.numfiles == 0) {
            // no files, so we need something more
            if (!settings.do_selftest && !settings.do_repl) {
                avmplus::AvmLog("You must provide input files, -repl, or -Dselftest, or the executable must be a projector file.\n");
                usage();
            }
        }
#endif
        
#ifdef VMCFG_EVAL
        if (settings.do_repl)
        {
            if (settings.numthreads > 1 || settings.numworkers > 1) {
                avmplus::AvmLog("The REPL requires exactly one worker on one thread.\n");
                usage();
            }
        }
#endif
        
#ifdef VMCFG_SELFTEST
        if (settings.do_selftest)
        {
            // Presumably we'd want to use the selftest harness to test multiple workers eventually.
            if (settings.numthreads > 1 || settings.numworkers > 1 || settings.numfiles > 0) {
                avmplus::AvmLog("The selftest harness requires exactly one worker on one thread, and no input files.\n");
                usage();
            }
        }
#endif
    }
    
    /* static */
    void Shell::repl(ShellCore* shellCore)
    {
        const int kMaxCommandLine = 1024;
        char commandLine[kMaxCommandLine];
        avmplus::String* input;
        
        avmplus::AvmLog("avmplus interactive shell\n"
                        "Type '?' for help\n\n");
        
        for (;;)
        {
            bool record_time = false;
            avmplus::AvmLog("> ");
            
            if(Platform::GetInstance()->getUserInput(commandLine, kMaxCommandLine) == NULL)
                return;
            
            commandLine[kMaxCommandLine-1] = 0;
            if (VMPI_strncmp(commandLine, "?", 1) == 0) {
                avmplus::AvmLog("Text entered at the prompt is compiled and evaluated unless\n"
                                "it is one of these commands:\n\n"
                                "  ?             print help\n"
                                "  .input        collect lines until a line that reads '.end',\n"
                                "                then eval the collected lines\n"
                                "  .load file    load the file (source or compiled)\n"
                                "  .quit         leave the repl\n"
                                "  .time expr    evaluate expr and report the time it took.\n\n");
                continue;
            }
            
            if (VMPI_strncmp(commandLine, ".load", 5) == 0) {
                const char* s = commandLine+5;
                while (*s == ' ' || *s == '\t')
                    s++;
                // wrong, handles only source code
                //readFileForEval(NULL, newStringLatin1(s));
                // FIXME: implement .load
                // Small amount of generalization of the code currently in the main loop should
                // take care of it.
                avmplus::AvmLog("The .load command is not implemented\n");
                continue;
            }
            
            if (VMPI_strncmp(commandLine, ".input", 6) == 0) {
                input = shellCore->newStringLatin1("");
                for (;;) {
                    if(Platform::GetInstance()->getUserInput(commandLine, kMaxCommandLine) == NULL)
                        return;
                    commandLine[kMaxCommandLine-1] = 0;
                    if (VMPI_strncmp(commandLine, ".end", 4) == 0)
                        break;
                    input->appendLatin1(commandLine);
                }
                goto compute;
            }
            
            if (VMPI_strncmp(commandLine, ".quit", 5) == 0) {
                return;
            }
            
            if (VMPI_strncmp(commandLine, ".time", 5) == 0) {
                record_time = true;
                input = shellCore->newStringLatin1(commandLine+5);
                goto compute;
            }
            
            input = shellCore->newStringLatin1(commandLine);
            
        compute:
            shellCore->evaluateString(input, record_time);
        }
    }
    
    /*static*/
    void Shell::usage()
    {
        avmplus::AvmLog("avmplus shell " AVMPLUS_VERSION_USER " " AVMPLUS_BIN_TYPE " build " AVMPLUS_BUILD_CODE "\n\n");
        Platform::GetInstance()->exit(1);
    }
    
    struct MultiworkerState;
    
    struct CoreNode
    {
        CoreNode(ShellCore* core, int id)
        : core(core)
        , id(id)
        , next(NULL)
        {
        }
        
        ~CoreNode()
        {
            // Destruction order matters.
            MMgc::GC* gc = core->GetGC();
            {
                MMGC_GCENTER(gc);
#ifdef _DEBUG
                core->codeContextThread = VMPI_currentThread();
#endif
                delete core;
            }
            
            
            delete gc;
        }
        
        ShellCore * const   core;
        const int           id;
        CoreNode *          next;       // For the LRU list of available cores
    };
    
    struct ThreadNode
    {
        ThreadNode(MultiworkerState& state, int id)
        : state(state)
        , id(id)
        , pendingWork(false)
        , corenode(NULL)
        , filename(NULL)
        , next(NULL)
        {
        }
        
        // Called from master, which should not be holding
        // thread_monitor but may hold global_monitor
        void startWork(CoreNode* corenode, const char* filename)
        {
            SCOPE_LOCK_NAMED(locker, thread_monitor) {
                this->corenode = corenode;
                this->filename = filename;
                this->pendingWork = true;
                locker.notify();
            }
        }
        
        vmbase::WaitNotifyMonitor thread_monitor;
        MultiworkerState& state;
        vmbase::VMThread* thread;
        const int id;
        bool pendingWork;
        CoreNode* corenode;         // The core running (or about to run, or just finished running) on this thread
        const char* filename;       // The work given to that core
        ThreadNode * next;          // For the LRU list of available threads
    };
    
    struct MultiworkerState
    {
        MultiworkerState(ShellSettings& settings)
        : settings(settings)
        , numthreads(settings.numthreads)
        , numcores(settings.numworkers)
        , free_threads(NULL)
        , free_threads_last(NULL)
        , free_cores(NULL)
        , free_cores_last(NULL)
        , num_free_threads(0)
        {
        }
        
        
        // Called from the slave threads, which should be holding
        // neither thread_monitor nor global_monitor.
        void freeThread(ThreadNode* t)
        {
            SCOPE_LOCK_NAMED(locker, global_monitor) {
                
                if (free_threads_last != NULL)
                    free_threads_last->next = t;
                else
                    free_threads = t;
                free_threads_last = t;
                
                if (t->corenode != NULL) {
                    if (free_cores_last != NULL)
                        free_cores_last->next = t->corenode;
                    else
                        free_cores = t->corenode;
                    free_cores_last = t->corenode;
                }
                
                num_free_threads++;
                
                locker.notify();
            }
        }
        
        // Called from the master thread, which must already hold global_monitor.
        bool getThreadAndCore(ThreadNode** t, CoreNode** c)
        {
            if (free_threads == NULL || free_cores == NULL)
                return false;
            
            *t = free_threads;
            free_threads = free_threads->next;
            if (free_threads == NULL)
                free_threads_last = NULL;
            (*t)->next = NULL;
            
            *c = free_cores;
            free_cores = free_cores->next;
            if (free_cores == NULL)
                free_cores_last = NULL;
            (*c)->next = NULL;
            
            num_free_threads--;
            
            return true;
        }
        
        vmbase::WaitNotifyMonitor global_monitor;
        ShellSettings&      settings;
        int                 numthreads;
        int                 numcores;
        
        // Queues of available threads and cores.  Protected by
        // global_monitor, also used to signal availability
        ThreadNode*         free_threads;
        ThreadNode*         free_threads_last;
        CoreNode*           free_cores;
        CoreNode*           free_cores_last;
        int                 num_free_threads;
    };
    
    static void masterThread(MultiworkerState& state);
    
    class SlaveThread : public vmbase::VMThread
    {
    public:
        SlaveThread(ThreadNode* tn) : self(tn) {}
        
        virtual void run();
        
    private:
        ThreadNode* self;
        
    };

    /* static */
    void Shell::multiWorker(ShellSettings& settings)
    {
        AvmAssert(!settings.do_repl && !settings.do_projector && !settings.do_selftest);
        
        MultiworkerState    state(settings);
        const int           numthreads(state.numthreads);
        const int           numcores(state.numcores);
        ThreadNode** const  threads(new ThreadNode*[numthreads]);
        CoreNode** const    cores(new CoreNode*[numcores]);
        
        MMgc::GCConfig gcconfig;
        gcconfig.collectionThreshold = settings.gcthreshold;
        gcconfig.exactTracing = settings.exactgc;
        gcconfig.markstackAllowance = settings.markstackAllowance;
        gcconfig.mode = settings.gcMode();
        
        // Going multi-threaded.
        
        // Create and start threads.  They add themselves to the free list.
        for ( int i=0 ; i < numthreads ; i++ )  {
            threads[i] = new ThreadNode(state, i);
            threads[i]->thread = new SlaveThread(threads[i]);
            threads[i]->thread->start();
        }
        
        // Create collectors and cores.
        // Extra credit: perform setup in parallel on the threads.
        for ( int i=0 ; i < numcores ; i++ ) {
            MMgc::GC* gc = new MMgc::GC(MMgc::GCHeap::GetGCHeap(),  gcconfig);
            MMGC_GCENTER(gc);
            cores[i] = new CoreNode(new ShellCoreImpl(gc, settings, false), i);
            if (!cores[i]->core->setup(settings))
                Platform::GetInstance()->exit(1);
        }
        
        // Add the cores to the free list.
        for ( int i=numcores-1 ; i >= 0 ; i-- ) {
            cores[i]->next = state.free_cores;
            state.free_cores = cores[i];
        }
        state.free_cores_last = cores[numcores-1];
        
        // No locks are held by the master at this point
        masterThread(state);
        // No locks are held by the master at this point
        
        // Some threads may still be computing, so just wait for them
        SCOPE_LOCK_NAMED(locker, state.global_monitor) {
            while (state.num_free_threads < numthreads)
                locker.wait();
        }
        
        // Shutdown: feed NULL to all threads to make them exit.
        for ( int i=0 ; i < numthreads ; i++ )
            threads[i]->startWork(NULL,NULL);
        
        // Wait for all threads to exit.
        for ( int i=0 ; i < numthreads ; i++ ) {
            threads[i]->thread->join();
            LOGGING( avmplus::AvmLog("T%d: joined the main thread\n", i); )
        }
        
        // Single threaded again.
        
        for ( int i=0 ; i < numthreads ; i++ ) {
            delete threads[i]->thread;
            delete threads[i];
        }
        
        for ( int i=0 ; i < numcores ; i++ )
            delete cores[i];
        
        delete [] threads;
        delete [] cores;
    }
    
    static void masterThread(MultiworkerState& state)
    {
        const int numfiles(state.settings.numfiles);
        const int repeats(state.settings.repeats);
        char** const filenames(state.settings.filenames);
        
        SCOPE_LOCK_NAMED(locker, state.global_monitor) {
            int r=0;
            for (bool finish = false; !finish;) {
                int nextfile = 0;
                for (;;) {
                    ThreadNode* threadnode;
                    CoreNode* corenode;
                    while (state.getThreadAndCore(&threadnode, &corenode) && !finish) {
                        LOGGING( avmplus::AvmLog("Scheduling %s on T%d with C%d\n", filenames[nextfile], threadnode->id, corenode->id); )
                        threadnode->startWork(corenode, filenames[nextfile]);
                        nextfile++;
                        if (nextfile == numfiles) {
                            r++;
                            if (r == repeats)
                                finish = true;
                            else
                                nextfile = 0;
                        }
                    }
                    // The original algorithm was jumping directly to
                    // the end of critical section upon discovering
                    // that it should terminate all the
                    // loops. Consequently, we have to break out of
                    // this for loop to bypass the locker.wait()
                    // statement.
                    if (finish) break;
                    locker.wait();
                }
            }
        }
    }
    
    void SlaveThread::run()
    {
        MMGC_ENTER_VOID;
        
        MultiworkerState& state = self->state;
        
        for (;;) {
            // Signal that we're ready for more work: add self to the list of free threads
            
            state.freeThread(self);
            
            // Obtain more work.  We have to hold self->thread_monitor here but the master won't touch corenode and
            // filename until we register for more work, so they don't have to be copied out of
            // the thread structure.
            
            SCOPE_LOCK_NAMED(locker, self->thread_monitor) {
                // Don't wait when pendingWork == true,
                // slave might have been already signalled but it didn't notice because it wasn't waiting yet.
                while (self->pendingWork == false)
                    locker.wait();
            }
            if (self->corenode == NULL) {
                LOGGING( avmplus::AvmLog("T%d: Exiting\n", self->id); )
                return;
            }
            
            // Perform work
            LOGGING( avmplus::AvmLog("T%d: Work starting\n", self->id); )
            {
                MMGC_GCENTER(self->corenode->core->GetGC());
#ifdef _DEBUG
                self->corenode->core->codeContextThread = VMPI_currentThread();
#endif
                self->corenode->core->evaluateFile(state.settings, self->filename); // Ignore the exit code for now
            }
            LOGGING( avmplus::AvmLog("T%d: Work completed\n", self->id); )
            
            SCOPE_LOCK(self->thread_monitor) {
                self->pendingWork = false;
            }
            
            
        }
        return;
    }
}
