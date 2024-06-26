/* The specs have two cases: NEXT_PDO, where binaries are thin, and Mach, 
 * where binaries can be fat.  In both cases, there may be a cpp-precomp
 * available, so tell the remainder of the driver code. */
#define NEXT_CPP_PRECOMP

#ifdef NEXT_PDO
#undef NEXT_SPEC
#define NEXT_SPEC(lang_flag,predef_macros,compiler_flags)\
   " %{traditional:cpp}%{traditional-cpp:%{!traditional:cpp}}\
    %{!traditional:%{!traditional-cpp:cpp-precomp -smart}}\
    %{.m:%BCompiling}%{!.m:%BCompiling} " # lang_flag "\
        %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d}\
        -undef -D__GNUC__=%v1 -D__GNUC_MINOR__=%v2\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	" # predef_macros " %{!undef:%{!ansi:%p} %P} %{trigraphs} \
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{precomp} %{no-precomp} \
        %{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
        %i %{!M:%{!MM:%{!E:%{!precomp:%{!pipe:%g.i}}}}}\
	%{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}}\
	%{precomp:%{@:-o %f%g-%T.p}%{!@:%W{o}%W{!o:-o %b.p }} } |\n",\
   "%{!M:%{!MM:%{!E:%{!precomp:cc1obj %{!pipe:%g.i} %1\
		   %{arch:} %{.m:-fobjc} %{ObjC:-fobjc}\
		   %{!Q:-quiet} -dumpbase %b.%{.m:m}%{!.m:c} %{d*} %{m*} %{a}\
		   %{g*} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
		   %{W*} %{w} %{pedantic*} %{ansi} \
		   %{traditional} %{v:-version} %{pg:-p} %{p} %{f*}\
		   %{aux-info*} " # compiler_flags "\
		   %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		   %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
              %{!S:as %{R} %{j} %{J} %{h} %{d2} %a %Y\
		      %{c:%W{o*}%{!o*:-o %w%b.o}}%{!c:-o %d%w%u.o}\
                      %{!pipe:%g.s} %A\n }}}}}"
  {".c", "@c"},
  {"@c",
   NEXT_SPEC (%{ObjC:-lang-objc} %{fobjc:-lang-objc}%{!ObjC:%{!fobjc:-lang-c}},
	      %{ObjC:-D__OBJC__} %{fobjc:-D__OBJC__},
	      %{ObjC:-fobjc %{gen-decls}} %{fobjc:%{gen-decls}})},
  {".m", "@objective-c"},
  {"@objective-c",
   NEXT_SPEC (-lang-objc, -D__OBJC__, -fobjc %{gen-decls})},
  {"-",
   "%{E:cpp -lang-c %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d}\
        -undef %{ObjC:-D__OBJC__} %{fobjc:-D__OBJC__}\
	%{fvec:-D__VEC__=10202}\
	-D__GNUC__=%v1 -D__GNUC_MINOR__=%v2  %{precomp} %{no-precomp}\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	%{!undef:%{!ansi:%p} %P} %{trigraphs}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
        %i %W{o*}}\
    %{!E:%e-E required when input is from standard input}"},
  {".h", "@c-header"},
#ifdef _WIN32
  {"@c-header",
   "%{!E:%{!precomp:%eCompilation of header file requested}}\
    %{.h:%BPrecompiling} \
    %{traditional:cpp}%{traditional-cpp:%{!traditional:cpp}}\
    %{!traditional:%{!traditional-cpp:cpp-precomp -smart}}\
    %{fno-objc:-lang-c}%{!fno-objc:-lang-objc} \
    %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	 %{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} \
        -undef -D__GNUC__=%v1 -D__GNUC_MINOR__=%v2\
	%{fvec:-D__VEC__=10202}\
	 %{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	%{!undef:%{!ansi:%p} %P} %{trigraphs}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
        %i %{!precomp:%W{o*}}%{precomp:%W{o*}%W{!o:-o %b.p}}"},
#else
  {"@c-header",
   "%{!E:%eCompilation of header file requested}\
    %{fno-objc:-lang-c}%{!fno-objc:-lang-objc} \
    %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	 %{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} \
        -undef -D__GNUC__=%v1 -D__GNUC_MINOR__=%v2\
	%{fvec:-D__VEC__=10202}\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	%{!undef:%{!ansi:%p} %P} %{trigraphs}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
        %i %W{o*}"},
#endif
#undef NEXT_SPEC
#define NEXT_SPEC(lang_flag,predef_macros,compiler_name,compiler_flags) \
   "cpp %{.M:%BCompiling}%{!.M:%BCompiling} " # lang_flag "\
        %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C++ does not support -C without using -E}}\
        %{precomp:%ePrecompilation of C++ not supported}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} \
	-undef -D__GNUC__=%v1 -D__GNUG__=%v1 -D__cplusplus -D__GNUC_MINOR__=%v2\
	" # predef_macros "\
	%{fvec:-D__VEC__=10202}\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__} %{!undef:%{!ansi:%p} %P}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional} %{trigraphs}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
        %i %{!M:%{!MM:%{!E:%{!pipe:%g.i}}}}%{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}} |\n",\
   "%{!M:%{!MM:%{!E:" # compiler_name " %{!pipe:%g.i} %1 %2\
		   %{!Q:-quiet} -dumpbase %b.%{.M:M}%{.mm:mm}%{!.M:%{!.mm:cc}}\
		   %{d*} %{m*} %{a}\
		   %{g*} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
		   %{W*} %{w} %{pedantic*} %{ansi} %{traditional}\
		   %{v:-version} %{pg:-p} %{p} %{f*} %{+e*}\
		   %{aux-info*} " # compiler_flags "\
		   %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		   %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
              %{!S:as %{R} %{j} %{J} %{h} %{d2} %a %Y\
		      %{c:%W{o*}%{!o*:-o %w%b.o}}%{!c:-o %d%w%u.o}\
                      %{!pipe:%g.s} %A\n }}}}"
  {".cc", "@c++"},
  {".cxx", "@c++"},
  {".cpp", "@c++"},
  {".C", "@c++"},
  {"@c++",
   NEXT_SPEC (%{fobjc:-lang-objc++}%{!fobjc:-lang-c++},
	      %{fobjc:-D__OBJC__},
	      cc1plus,
	      %{fobjc:%{gen-decls}})},
  {".M", "@objective-c++"},
  {".mm", "@objective-c++"},
  {"@objective-c++",
   NEXT_SPEC (-lang-objc++, -D__OBJC__, cc1objplus, -fobjc %{gen-decls})},
  {".i", "@cpp-output"},
  {"@cpp-output",
   "cc1obj %{.i:%BCompiling} %i %1 %{!Q:-quiet} %{d*} %{m*} %{a}\
	%{g*} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
	%{W*} %{w} %{pedantic*} %{ansi} %{traditional}\
	%{v:-version} %{pg:-p} %{p} %{f*}\
	%{aux-info*}\
	%{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
	%{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
    %{!S:as %{R} %{j} %{J} %{h} %{d2} %a %Y\
            %{c:%W{o*}%{!o*:-o %w%b.o}}%{!c:-o %d%w%u.o} %{!pipe:%g.s} %A\n }"},
  {".ii", "@c++-cpp-output"},
  {"@c++-cpp-output",
   "cc1objplus %{.ii:%BCompiling} %i %1 %2 %{!Q:-quiet} %{d*} %{m*} %{a}\
	    %{g*} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
	    %{W*} %{w} %{pedantic*} %{ansi} %{traditional}\
	    %{v:-version} %{pg:-p} %{p} %{f*} %{+e*}\
	    %{aux-info*}\
	    %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
	    %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
       %{!S:as %{R} %{j} %{J} %{h} %{d2} %a %Y\
	       %{c:%W{o*}%{!o*:-o %w%b.o}}%{!c:-o %d%w%u.o}\
	       %{!pipe:%g.s} %A\n }"},
  {".s", "@assembler-with-cpp"},
  {".S", "@assembler-with-cpp"},
  {"@assembler-with-cpp",
   "cpp -lang-asm %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{.s:%BAssembling} %{.S:%BAssembling}\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{trigraphs} \
        -undef -$ %{!undef:%p %P} -D__ASSEMBLER__ \
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
        %i %{!M:%{!MM:%{!E:%{!pipe:%g.s}}}}%{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}} |\n",
   "%{!M:%{!MM:%{!E:%{!S:as %{R} %{j} %{J} %{h} %{d2} %a %Y\
                    %{c:%W{o*}%{!o*:-o %w%b.o}}%{!c:-o %d%w%u.o}\
		    %{!pipe:%g.s} %A\n }}}}"},
#else
#undef NEXT_SPEC
#define NEXT_SPEC(lang_flag,predef_macros,compiler_flags)\
   "%{@:%{E:%eCannot use -E with multiple architectures}\
	%{M:%eCannot use -M with multiple architectures}\
	%{MM:%eCannot use -MM with multiple architectures}\
	%{S:%eCannot use -S with multiple architectures}}\
    %{traditional:cpp}\
    %{!traditional:%{traditional-cpp:cpp}\
		   %{!traditional-cpp:cpp-precomp -smart %{precomp-trustfile}}}\
    %{.m:%BCompiling}%{!.m:%BCompiling} " # lang_flag "\
    %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	-arch %T %{@:-arch_multiple}\
	%{M} %{MM} %{MD:-MD %M} %{MMD:-MMD %M}\
	-undef -D__GNUC__=%v1 -D__GNUC_MINOR__=%v2\
	%{fvec:-D__VEC__=10202}\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	" # predef_macros " %{precomp} %{no-precomp}\
	%{!undef:%{!ansi:%p} %P} %{trigraphs}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
	%i %{!M:%{!MM:%{!E:%{!precomp:%{!pipe:%g.i}}}}}\
	%{E:%W{o}}%{M:%W{o}}%{MM:%W{o}}\
	%{precomp:%{@:-o %f%g-%T.p}%{!@:%W{o}%W{!o:-o %b.p }} } |\n",\
   "%{!M:%{!MM:%{!E:%{!precomp:cc1obj %{!pipe:%g.i} %1 \
		   -arch %T %{@:-arch_multiple} %{arch:} \
		   %{!Q:-quiet} -dumpbase %b.%{.m:m}%{!.m:c} %{d*} %{m*} %{a}\
		   %{g*} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
		   %{W*} %{w} %{pedantic*} %{ansi} \
		   %{traditional} %{v:-version} %{pg:-p} %{p} %{f*}\
		   %{aux-info*} %{!static:%{!dynamic:-dynamic }-fPIC}\
		   %{threeThreeMethodEncoding} " # compiler_flags "\
		   %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		   %{S:%W{o}%{!o:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
	      %{!S:as %{I*} %{R} %{j} %{J} %{h} %{d2} %a %Y\
		      %{force_cpusubtype_ALL} %{dynamic} %{static} \
 		      %{NEXTSTEP-deployment-target} \
		      -arch %T %{@:-arch_multiple}\
		      %{@:-o %f%g-%T.o}%{!@: %{c:%W{o}%{!o:-o %w%b.o}}%{!c:-o %d%w%g-%b.o}}\
		      %{!pipe:%g.s} %A\n }}}}}"
  {".c", "@c"},
  {"@c",
   NEXT_SPEC (%{fobjc:-lang-objc}%{!fobjc:-lang-c},
	      %{fobjc:-D__OBJC__},
	      %{fobjc:%{gen-decls}})},
  {".m", "@objective-c"},
  {"@objective-c",
   NEXT_SPEC (-lang-objc, -D__OBJC__, -fobjc %{gen-decls})},
  {"-",
   "%{@:%{E:%eCannot use -E with multiple architectures}\
	%{M:%eCannot use -M with multiple architectures}\
	%{MM:%eCannot use -MM with multiple architectures}\
	%{S:%eCannot use -S with multiple architectures}}\
    %{E:cpp %{ObjC:-lang-objc} %{fobjc:-lang-objc} %{!ObjC:%{!fobjc:-lang-c}}\
 	%{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %M} %{MMD:-MMD %M}\
	%{precomp} %{no-precomp}\
	-undef %{ObjC:-D__OBJC__} %{fobjc:-D__OBJC__}\
	-D__GNUC__=%v1 -D__GNUC_MINOR__=%v2\
	%{fvec:-D__VEC__=10202}\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	%{!undef:%{!ansi:%p} %P} %{trigraphs}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
	%i %W{o}}\
    %{!E:%e-E required when input is from standard input}"},
  {".h", "@c-header"},
  {"@c-header",
   "%{@:%{E:%eCannot use -E with multiple architectures}\
	%{M:%eCannot use -M with multiple architectures}\
	%{MM:%eCannot use -MM with multiple architectures}\
	%{S:%eCannot use -S with multiple architectures}}\
    %{!E:%{!precomp:%eCompilation of header file requested}} \
        %{.h:%BPrecompiling} \
	%{traditional:cpp}\
	%{!traditional:%{traditional-cpp:cpp}\
		       %{!traditional-cpp:cpp-precomp -smart %{precomp-trustfile}}}\
	%{fno-objc:-lang-c}%{!fno-objc:-lang-objc} \
    	%{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	-arch %T %{@:-arch_multiple}\
	%{M} %{MM} %{MD:-MD %M} %{MMD:-MMD %M} \
	%{precomp} %{no-precomp}\
	-undef -D__GNUC__=%v1 -D__GNUC_MINOR__=%v2\
	%{fvec:-D__VEC__=10202}\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	%{ObjC:-D__OBJC__} %{fobjc:-D__OBJC__} \
	%{!undef:%{!ansi:%p} %P} %{trigraphs}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
	%i %{!precomp:%W{o}}%{precomp:%{@:-o %f%g-%T.p}%{!@:%W{o}%W{!o:-o %b.p }} }"},
#undef NEXT_SPEC
#define NEXT_SPEC(lang_flag,predef_macros,compiler_name,compiler_flags) \
   "%{@:%{E:%eCannot use -E with multiple architectures}\
	%{M:%eCannot use -M with multiple architectures}\
	%{MM:%eCannot use -MM with multiple architectures}\
	%{S:%eCannot use -S with multiple architectures}}\
    cpp %{.M:%BCompiling}%{!.M:%BCompiling} " # lang_flag "\
 	%{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C++ does not support -C without using -E}}\
	%{precomp:%ePrecompilation of C++ not supported}\
	%{M} %{MM} %{MD:-MD %M} %{MMD:-MMD %M}\
	-undef -D__GNUC__=%v1 -D__GNUC_MINOR__=%v2 -D__GNUG__=%v1\
	-D__cplusplus " # predef_macros "\
	%{fvec:-D__VEC__=10202}\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__} %{!undef:%{!ansi:%p} %P}\
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
	%{trigraphs}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
	%i %{!M:%{!MM:%{!E:%{!precomp:%{!pipe:%g.i}}}}} %{E:%W{o}}%{M:%W{o}}%{MM:%W{o}}\
	%{precomp:%{@:-o %f%g-%T.p}%{!@:%W{o}%W{!o:-o %b.p }} } |\n",\
   "%{!M:%{!MM:%{!E:%{!precomp:" # compiler_name " %{!pipe:%g.i} %1 %2\
		   -arch %T %{@:-arch_multiple} \
		   %{!Q:-quiet} -dumpbase %b.%{.M:M}%{.mm:mm}%{!.M:%{!.mm:cc}}\
		   %{d*} %{m*} %{a}\
		   %{!g:%{g*}} %{g:-ggdb} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
		   %{W*} %{w} %{pedantic*} %{ansi} %{traditional}\
		   %{v:-version} %{pg:-p} %{p} %{f*} %{+e*}\
		   %{aux-info*} %{!static:%{!dynamic:-dynamic }-fPIC}\
		   %{threeThreeMethodEncoding} " # compiler_flags "\
		   %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		   %{S:%W{o}%{!o:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
	      %{!S:as %{I*} %{R} %{j} %{J} %{h} %{d2} %a %Y\
			    %{force_cpusubtype_ALL} %{dynamic} %{static} -arch %T %{@:-arch_multiple}\
			    %{@:-o %f%g-%T.o}%{!@: %{c:%W{o}%{!o:-o %w%b.o}}%{!c:-o %d%w%g-%b.o}}\
			    %{!pipe:%g.s} %A\n }}}}}"
  {".C", "@c++"},
  {".cc", "@c++"},
  {".cp", "@c++"},
  {".cpp", "@c++"},
  {".cxx", "@c++"},
  {"@c++",
   NEXT_SPEC (%{fobjc:-lang-objc++}%{!fobjc:-lang-c++},
	      %{fobjc:-D__OBJC__},
	      cc1plus,
	      %{fobjc:%{gen-decls}})},
  {".M", "@objective-c++"},
  {".mm", "@objective-c++"},
  {"@objective-c++",
   NEXT_SPEC (-lang-objc++, -D__OBJC__, cc1objplus, -fobjc %{gen-decls})},
  {".i", "@cpp-output"},
  {"@cpp-output",
   "%{@:%{E:%eCannot use -E with multiple architectures}\
	%{M:%eCannot use -M with multiple architectures}\
	%{MM:%eCannot use -MM with multiple architectures}\
	%{S:%eCannot use -S with multiple architectures}}\
    cc1obj %i %1 %{.i:%BCompiling} \
	-arch %T %{@:-arch_multiple} %{ObjC:-fobjc} %{fobjc} \
	%{!Q:-quiet} %{Y*} %{d*} %{m*} %{a}\
	%{g*} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
	%{W*} %{w} %{pedantic*} %{ansi} %{traditional}\
	%{v:-version} %{pg:-p} %{p} %{f*}\
	%{aux-info*} %{!static:%{!dynamic:-dynamic }-fPIC}\
	%{threeThreeMethodEncoding} %{Obj:%{gen-decls}} %{fobjc:%{gen-decls}}\
	%{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
	%{S:%W{o}%{!o:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
    %{!S:as %{I*} %{R} %{j} %{J} %{h} %{d2} %a %Y\
	    %{force_cpusubtype_ALL} %{dynamic} %{static} -arch %T %{@:-arch_multiple}\
	    %{@:-o %f%g-%T.o}%{!@: %{c:%W{o}%{!o:-o %w%b.o}}%{!c:-o %d%w%g-%b.o}} %{!pipe:%g.s} %A\n }"},
  {".ii", "@c++-cpp-output"},
  {"@c++-cpp-output",
   "cc1objplus %i %1 %{.ii:%BCompiling}\
	    -arch %T %{@:-arch_multiple} %{ObjC++:-fobjc} %{fobjc}\
	    %{!Q:-quiet} %{d*} %{m*} %{a}\
	    %{!g:%{g*}} %{g:-ggdb} %{O}%{O1}%{O2}%{O3}%{O4}%{O5}%{O6}\
	    %{W*} %{w} %{pedantic*} %{ansi} %{traditional}\
	    %{v:-version} %{pg:-p} %{p} %{f*} %{+e*}\
	    %{aux-info*} %{!static:%{!dynamic:-dynamic }-fPIC}\
	    %{threeThreeMethodEncoding} %{ObjC++:%{gen-decls}} %{fobjc:%{gen-decls}}\
	    %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
	    %{S:%W{o}%{!o:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
       %{!S:as %{I*} %{R} %{j} %{J} %{h} %{d2} %a %Y\
	       %{force_cpusubtype_ALL} %{dynamic} %{static} -arch %T %{@:-arch_multiple}\
	       %{@:-o %f%g-%T.o}%{!@: %{c:%W{o}%{!o:-o %w%b.o}}%{!c:-o %d%w%g-%b.o}}\
	       %{!pipe:%g.s} %A\n }"},
  {".s", "@assembler-with-cpp"},
  {".S", "@assembler-with-cpp"},
  {"@assembler-with-cpp",
   "%{@:%{E:%eCannot use -E with multiple architectures}\
	%{M:%eCannot use -M with multiple architectures}\
	%{MM:%eCannot use -MM with multiple architectures}\
	%{S:%eCannot use -S with multiple architectures}}\
    cpp -lang-asm %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
        %{.s:%BAssembling}%{.S:%BAssembling} \
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %M} %{MMD:-MMD %M} %{trigraphs} \
	%{precomp:%dPrecompilation of assembler not supported}\
	-undef -$ %{!undef:%p %P} -D__ASSEMBLER__ \
	%c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{traditional} %{ftraditional:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %{F*} %C %{D*} %{U*} %{i*}\
	%i %{!M:%{!MM:%{!E:%{!pipe:%g.s}}}}%{E:%W{o}}%{M:%W{o}}%{MM:%W{o}} |\n",
   "%{!M:%{!MM:%{!E:%{!S:as %{I*} %{R} %{j} %{J} %{h} %{d2} %a %Y\
		   %{force_cpusubtype_ALL} %{dynamic} %{static} -arch %T %{@:-arch_multiple} \
		    %{@:-o %f%g-%T.o}%{!@: %{c:%W{o}%{!o:-o %w%b.o}}%{!c:-o %d%w%g-%b.o}}\
		    %{!pipe:%g.s} %A\n }}}}"},
#endif
