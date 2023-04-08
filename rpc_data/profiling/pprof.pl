#! /usr/bin/env perl
use strict;
use warnings;
use Getopt::Long;
use Cwd;
use POSIX;

my $PPROF_VERSION = "2.0";

# These are the object tools we use which can come from a
# user-specified location using --tools, from the PPROF_TOOLS
# environment variable, or from the environment.
my %obj_tool_map = (
  "objdump" => "objdump",
  "nm" => "nm",
  "addr2line" => "addr2line",
  "c++filt" => "c++filt",
  ## ConfigureObjTools may add architecture-specific entries:
  #"nm_pdb" => "nm-pdb",       # for reading windows (PDB-format) executables
  #"addr2line_pdb" => "addr2line-pdb",                                # ditto
  #"otool" => "otool",         # equivalent of objdump on OS X
);
# NOTE: these are lists, so you can put in commandline flags if you want.
my @DOT = ("dot");          # leave non-absolute, since it may be in /usr/local
my @GV = ("gv");
my @EVINCE = ("evince");    # could also be xpdf or perhaps acroread
my @KCACHEGRIND = ("kcachegrind");
my @PS2PDF = ("ps2pdf");
# These are used for dynamic profiles
my @URL_FETCHER = ("curl", "-s");

# These are the web pages that servers need to support for dynamic profiles
my $HEAP_PAGE = "/pprof/heap";
my $PROFILE_PAGE = "/pprof/profile";   # must support cgi-param "?seconds=#"
my $PMUPROFILE_PAGE = "/pprof/pmuprofile(?:\\?.*)?"; # must support cgi-param
                                                # ?seconds=#&event=x&period=n
my $GROWTH_PAGE = "/pprof/growth";
my $CONTENTION_PAGE = "/pprof/contention";
my $WALL_PAGE = "/pprof/wall(?:\\?.*)?";  # accepts options like namefilter
my $FILTEREDPROFILE_PAGE = "/pprof/filteredprofile(?:\\?.*)?";
my $CENSUSPROFILE_PAGE = "/pprof/censusprofile(?:\\?.*)?"; # must support cgi-param
                                                       # "?seconds=#",
                                                       # "?tags_regexp=#" and
                                                       # "?type=#".
my $SYMBOL_PAGE = "/pprof/symbol";     # must support symbol lookup via POST
my $PROGRAM_NAME_PAGE = "/pprof/cmdline";

# These are the web pages that can be named on the command line.
# All the alternatives must begin with /.
my $PROFILES = "($HEAP_PAGE|$PROFILE_PAGE|$PMUPROFILE_PAGE|" .
               "$GROWTH_PAGE|$CONTENTION_PAGE|$WALL_PAGE|" .
               "$FILTEREDPROFILE_PAGE|$CENSUSPROFILE_PAGE)";

# default binary name
my $UNKNOWN_BINARY = "(unknown)";

# There is a pervasive dependency on the length (in hex characters,
# i.e., nibbles) of an address, distinguishing between 32-bit and
# 64-bit profiles.  To err on the safe size, default to 64-bit here:
my $address_length = 16;

my $dev_null = "/dev/null";
if (! -e $dev_null && $^O =~ /MSWin/) {    # $^O is the OS perl was built for
  $dev_null = "nul";
}

# A list of paths to search for shared object files
my @prefix_list = ();

# Special routine name that should not have any symbols.
# Used as separator to parse "addr2line -i" output.
my $sep_symbol = '_fini';
my $sep_address = undef;

my @stackTraces;

##### Argument parsing #####

sub usage_string {
  return <<EOF;
Usage:
$0 [options] <program> <profiles>
   <profiles> is a space separated list of profile names.
$0 [options] <symbolized-profiles>
   <symbolized-profiles> is a list of profile files where each file contains
   the necessary symbol mappings  as well as profile data (likely generated
   with --raw).
$0 [options] <profile>
   <profile> is a remote form.  Symbols are obtained from host:port$SYMBOL_PAGE
   Each name can be:
   /path/to/profile        - a path to a profile file
   host:port[/<service>]   - a location of a service to get profile from
   The /<service> can be $HEAP_PAGE, $PROFILE_PAGE, /pprof/pmuprofile,
                         $GROWTH_PAGE, $CONTENTION_PAGE, /pprof/wall,
                         $CENSUSPROFILE_PAGE, or /pprof/filteredprofile.
   For instance:
     $0 http://myserver.com:80$HEAP_PAGE
   If /<service> is omitted, the service defaults to $PROFILE_PAGE (cpu profiling).
$0 --symbols <program>
   Maps addresses to symbol names.  In this mode, stdin should be a
   list of library mappings, in the same format as is found in the heap-
   and cpu-profile files (this loosely matches that of /proc/self/maps
   on linux), followed by a list of hex addresses to map, one per line.
   For more help with querying remote servers, including how to add the
   necessary server-side support code, see this filename (or one like it):
   /usr/doc/gperftools-$PPROF_VERSION/pprof_remote_servers.html
Options:
   --cum               Sort by cumulative data
   --base=<base>       Subtract <base> from <profile> before display
   --interactive       Run in interactive mode (interactive "help" gives help) [default]
   --seconds=<n>       Length of time for dynamic profiles [default=30 secs]
   --add_lib=<file>    Read additional symbols and line info from the given library
   --lib_prefix=<dir>  Comma separated list of library path prefixes
   --no_strip_temp     Do not strip template arguments from function names
Reporting Granularity:
   --addresses         Report at address level
   --lines             Report at source line level
   --functions         Report at function level [default]
   --files             Report at source file level
Output type:
   --text              Generate text report
   --stacks            Generate stack traces similar to the heap profiler (requires --text)
   --callgrind         Generate callgrind format to stdout
   --gv                Generate Postscript and display
   --evince            Generate PDF and display
   --web               Generate SVG and display
   --list=<regexp>     Generate source listing of matching routines
   --disasm=<regexp>   Generate disassembly of matching routines
   --symbols           Print demangled symbol names found at given addresses
   --dot               Generate DOT file to stdout
   --ps                Generate Postscript to stdout
   --pdf               Generate PDF to stdout
   --svg               Generate SVG to stdout
   --gif               Generate GIF to stdout
   --raw               Generate symbolized pprof data (useful with remote fetch)
   --collapsed         Generate collapsed stacks for building flame graphs
                       (see http://www.brendangregg.com/flamegraphs.html)
Heap-Profile Options:
   --inuse_space       Display in-use (mega)bytes [default]
   --inuse_objects     Display in-use objects
   --alloc_space       Display allocated (mega)bytes
   --alloc_objects     Display allocated objects
   --show_bytes        Display space in bytes
   --drop_negative     Ignore negative differences
Contention-profile options:
   --total_delay       Display total delay at each region [default]
   --contentions       Display number of delays at each region
   --mean_delay        Display mean delay at each region
Call-graph Options:
   --nodecount=<n>     Show at most so many nodes [default=80]
   --nodefraction=<f>  Hide nodes below <f>*total [default=.005]
   --edgefraction=<f>  Hide edges below <f>*total [default=.001]
   --maxdegree=<n>     Max incoming/outgoing edges per node [default=8]
   --focus=<regexp>    Focus on nodes matching <regexp>
   --ignore=<regexp>   Ignore nodes matching <regexp>
   --scale=<n>         Set GV scaling [default=0]
   --heapcheck         Make nodes with non-0 object counts
                       (i.e. direct leak generators) more visible
Miscellaneous:
   --no-auto-signal-frm Automatically drop 2nd frame that is always same (cpu-only)
                       (assuming that it is artifact of bad stack captures
                        which include signal handler frames)
   --show_addresses    Always show addresses when applicable
   --tools=<prefix or binary:fullpath>[,...]   \$PATH for object tool pathnames
   --test              Run unit tests
   --help              This message
   --version           Version information
Environment Variables:
   PPROF_TMPDIR        Profiles directory. Defaults to \$HOME/pprof
   PPROF_TOOLS         Prefix for object tools pathnames
Examples:
$0 /bin/ls ls.prof
                       Enters "interactive" mode
$0 --text /bin/ls ls.prof
                       Outputs one line per procedure
$0 --web /bin/ls ls.prof
                       Displays annotated call-graph in web browser
$0 --gv /bin/ls ls.prof
                       Displays annotated call-graph via 'gv'
$0 --gv --focus=Mutex /bin/ls ls.prof
                       Restricts to code paths including a .*Mutex.* entry
$0 --gv --focus=Mutex --ignore=string /bin/ls ls.prof
                       Code paths including Mutex but not string
$0 --list=getdir /bin/ls ls.prof
                       (Per-line) annotated source listing for getdir()
$0 --disasm=getdir /bin/ls ls.prof
                       (Per-PC) annotated disassembly for getdir()
$0 http://localhost:1234/
                       Enters "interactive" mode
$0 --text localhost:1234
                       Outputs one line per procedure for localhost:1234
$0 --raw localhost:1234 > ./local.raw
$0 --text ./local.raw
                       Fetches a remote profile for later analysis and then
                       analyzes it in text mode.
EOF
}

sub version_string {
  return <<EOF
pprof (part of gperftools $PPROF_VERSION)
Copyright 1998-2007 Google Inc.
This is BSD licensed software; see the source for copying conditions
and license information.
There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.
EOF
}

sub usage {
  my $msg = shift;
  print STDERR "$msg\n\n";
  print STDERR usage_string();
  exit(1);
}

sub Init() {
  # Setup tmp-file name and handler to clean it up.
  # We do this in the very beginning so that we can use
  # error() and cleanup() function anytime here after.
  $main::tmpfile_sym = "/tmp/pprof$$.sym";
  $main::tmpfile_ps = "/tmp/pprof$$";
  $main::next_tmpfile = 0;
  $SIG{'INT'} = \&sighandler;

  # Cache from filename/linenumber to source code
  $main::source_cache = ();

  $main::opt_help = 0;
  $main::opt_version = 0;
  $main::opt_show_addresses = 0;
  $main::opt_no_auto_signal_frames = 0;

  $main::opt_cum = 0;
  $main::opt_base = '';
  $main::opt_addresses = 0;
  $main::opt_lines = 0;
  $main::opt_functions = 0;
  $main::opt_files = 0;
  $main::opt_lib_prefix = "";

  $main::opt_text = 0;
  $main::opt_stacks = 0;
  $main::opt_callgrind = 0;
  $main::opt_list = "";
  $main::opt_disasm = "";
  $main::opt_symbols = 0;
  $main::opt_gv = 0;
  $main::opt_evince = 0;
  $main::opt_web = 0;
  $main::opt_dot = 0;
  $main::opt_ps = 0;
  $main::opt_pdf = 0;
  $main::opt_gif = 0;
  $main::opt_svg = 0;
  $main::opt_raw = 0;
  $main::opt_collapsed = 0;

  $main::opt_nodecount = 80;
  $main::opt_nodefraction = 0.005;
  $main::opt_edgefraction = 0.001;
  $main::opt_maxdegree = 8;
  $main::opt_focus = '';
  $main::opt_ignore = '';
  $main::opt_scale = 0;
  $main::opt_heapcheck = 0;
  $main::opt_seconds = 30;
  $main::opt_lib = "";

  $main::opt_inuse_space   = 0;
  $main::opt_inuse_objects = 0;
  $main::opt_alloc_space   = 0;
  $main::opt_alloc_objects = 0;
  $main::opt_show_bytes    = 0;
  $main::opt_drop_negative = 0;
  $main::opt_interactive   = 0;

  $main::opt_total_delay = 0;
  $main::opt_contentions = 0;
  $main::opt_mean_delay = 0;

  $main::opt_tools   = "";
  $main::opt_debug   = 0;
  $main::opt_test    = 0;

  # Do not strip template argument in function names
  $main::opt_no_strip_temp = 0;

  # These are undocumented flags used only by unittests.
  $main::opt_test_stride = 0;

  # Are we using $SYMBOL_PAGE?
  $main::use_symbol_page = 0;

  # Files returned by TempName.
  %main::tempnames = ();

  # Type of profile we are dealing with
  # Supported types:
  #     cpu
  #     heap
  #     growth
  #     contention
  $main::profile_type = '';     # Empty type means "unknown"

  GetOptions("help!"          => \$main::opt_help,
             "version!"       => \$main::opt_version,
             "show_addresses!"=> \$main::opt_show_addresses,
             "no-auto-signal-frm!"=> \$main::opt_no_auto_signal_frames,
             "cum!"           => \$main::opt_cum,
             "base=s"         => \$main::opt_base,
             "seconds=i"      => \$main::opt_seconds,
             "add_lib=s"      => \$main::opt_lib,
             "lib_prefix=s"   => \$main::opt_lib_prefix,
             "functions!"     => \$main::opt_functions,
             "lines!"         => \$main::opt_lines,
             "addresses!"     => \$main::opt_addresses,
             "files!"         => \$main::opt_files,
             "text!"          => \$main::opt_text,
             "stacks!"        => \$main::opt_stacks,
             "callgrind!"     => \$main::opt_callgrind,
             "list=s"         => \$main::opt_list,
             "disasm=s"       => \$main::opt_disasm,
             "symbols!"       => \$main::opt_symbols,
             "gv!"            => \$main::opt_gv,
             "evince!"        => \$main::opt_evince,
             "web!"           => \$main::opt_web,
             "dot!"           => \$main::opt_dot,
             "ps!"            => \$main::opt_ps,
             "pdf!"           => \$main::opt_pdf,
             "svg!"           => \$main::opt_svg,
             "gif!"           => \$main::opt_gif,
             "raw!"           => \$main::opt_raw,
             "collapsed!"     => \$main::opt_collapsed,
             "interactive!"   => \$main::opt_interactive,
             "nodecount=i"    => \$main::opt_nodecount,
             "nodefraction=f" => \$main::opt_nodefraction,
             "edgefraction=f" => \$main::opt_edgefraction,
             "maxdegree=i"    => \$main::opt_maxdegree,
             "focus=s"        => \$main::opt_focus,
             "ignore=s"       => \$main::opt_ignore,
             "scale=i"        => \$main::opt_scale,
             "heapcheck"      => \$main::opt_heapcheck,
             "inuse_space!"   => \$main::opt_inuse_space,
             "inuse_objects!" => \$main::opt_inuse_objects,
             "alloc_space!"   => \$main::opt_alloc_space,
             "alloc_objects!" => \$main::opt_alloc_objects,
             "show_bytes!"    => \$main::opt_show_bytes,
             "drop_negative!" => \$main::opt_drop_negative,
             "total_delay!"   => \$main::opt_total_delay,
             "contentions!"   => \$main::opt_contentions,
             "mean_delay!"    => \$main::opt_mean_delay,
             "tools=s"        => \$main::opt_tools,
             "no_strip_temp!" => \$main::opt_no_strip_temp,
             "test!"          => \$main::opt_test,
             "debug!"         => \$main::opt_debug,
             # Undocumented flags used only by unittests:
             "test_stride=i"  => \$main::opt_test_stride,
      ) || usage("Invalid option(s)");

  # Deal with the standard --help and --version
  if ($main::opt_help) {
    print usage_string();
    exit(0);
  }

  if ($main::opt_version) {
    print version_string();
    exit(0);
  }

  # Disassembly/listing/symbols mode requires address-level info
  if ($main::opt_disasm || $main::opt_list || $main::opt_symbols) {
    $main::opt_functions = 0;
    $main::opt_lines = 0;
    $main::opt_addresses = 1;
    $main::opt_files = 0;
  }

  # Check heap-profiling flags
  if ($main::opt_inuse_space +
      $main::opt_inuse_objects +
      $main::opt_alloc_space +
      $main::opt_alloc_objects > 1) {
    usage("Specify at most on of --inuse/--alloc options");
  }

  # Check output granularities
  my $grains =
      $main::opt_functions +
      $main::opt_lines +
      $main::opt_addresses +
      $main::opt_files +
      0;
  if ($grains > 1) {
    usage("Only specify one output granularity option");
  }
  if ($grains == 0) {
    $main::opt_functions = 1;
  }

  # Check output modes
  my $modes =
      $main::opt_text +
      $main::opt_callgrind +
      ($main::opt_list eq '' ? 0 : 1) +
      ($main::opt_disasm eq '' ? 0 : 1) +
      ($main::opt_symbols == 0 ? 0 : 1) +
      $main::opt_gv +
      $main::opt_evince +
      $main::opt_web +
      $main::opt_dot +
      $main::opt_ps +
      $main::opt_pdf +
      $main::opt_svg +
      $main::opt_gif +
      $main::opt_raw +
      $main::opt_collapsed +
      $main::opt_interactive +
      0;
  if ($modes > 1) {
    usage("Only specify one output mode");
  }
  if ($modes == 0) {
    if (-t STDOUT) {  # If STDOUT is a tty, activate interactive mode
      $main::opt_interactive = 1;
    } else {
      $main::opt_text = 1;
    }
  }

  if ($main::opt_test) {
    RunUnitTests();
    # Should not return
    exit(1);
  }

  # Binary name and profile arguments list
  $main::prog = "";
  @main::pfile_args = ();

  # Remote profiling without a binary (using $SYMBOL_PAGE instead)
  if (@ARGV > 0) {
    if (IsProfileURL($ARGV[0])) {
      printf STDERR "Using remote profile at $ARGV[0].\n";
      $main::use_symbol_page = 1;
    } elsif (IsSymbolizedProfileFile($ARGV[0])) {
      $main::use_symbolized_profile = 1;
      $main::prog = $UNKNOWN_BINARY;  # will be set later from the profile file
    }
  }

  if ($main::use_symbol_page || $main::use_symbolized_profile) {
    # We don't need a binary!
    my %disabled = ('--lines' => $main::opt_lines,
                    '--disasm' => $main::opt_disasm);
    for my $option (keys %disabled) {
      usage("$option cannot be used without a binary") if $disabled{$option};
    }
    # Set $main::prog later...
    scalar(@ARGV) || usage("Did not specify profile file");
  } elsif ($main::opt_symbols) {
    # --symbols needs a binary-name (to run nm on, etc) but not profiles
    $main::prog = shift(@ARGV) || usage("Did not specify program");
  } else {
    $main::prog = shift(@ARGV) || usage("Did not specify program");
    scalar(@ARGV) || usage("Did not specify profile file");
  }

  # Parse profile file/location arguments
  foreach my $farg (@ARGV) {
    if ($farg =~ m/(.*)\@([0-9]+)(|\/.*)$/ ) {
      my $machine = $1;
      my $num_machines = $2;
      my $path = $3;
      for (my $i = 0; $i < $num_machines; $i++) {
        unshift(@main::pfile_args, "$i.$machine$path");
      }
    } else {
      unshift(@main::pfile_args, $farg);
    }
  }

  if ($main::use_symbol_page) {
    unless (IsProfileURL($main::pfile_args[0])) {
      error("The first profile should be a remote form to use $SYMBOL_PAGE\n");
    }
    CheckSymbolPage();
    $main::prog = FetchProgramName();
  } elsif (!$main::use_symbolized_profile) {  # may not need objtools!
    ConfigureObjTools($main::prog)
  }

  # Break the opt_lib_prefix into the prefix_list array
  @prefix_list = split (',', $main::opt_lib_prefix);

  # Remove trailing / from the prefixes, in the list to prevent
  # searching things like /my/path//lib/mylib.so
  foreach (@prefix_list) {
    s|/+$||;
  }
}

sub Main() {
  Init();
  $main::collected_profile = undef;
  @main::profile_files = ();
  $main::op_time = time();

  # Printing symbols is special and requires a lot less info that most.
  if ($main::opt_symbols) {
    PrintSymbols(*STDIN);   # Get /proc/maps and symbols output from stdin
    return;
  }

  # Fetch all profile data
  FetchDynamicProfiles();

  # this will hold symbols that we read from the profile files
  my $symbol_map = {};

  # Read one profile, pick the last item on the list
  my $data = ReadProfile($main::prog, pop(@main::profile_files));
  my $profile = $data->{profile};
  my $pcs = $data->{pcs};
  my $libs = $data->{libs};   # Info about main program and shared libraries
  $symbol_map = MergeSymbols($symbol_map, $data->{symbols});

  # Add additional profiles, if available.
  if (scalar(@main::profile_files) > 0) {
    foreach my $pname (@main::profile_files) {
      my $data2 = ReadProfile($main::prog, $pname);
      $profile = AddProfile($profile, $data2->{profile});
      $pcs = AddPcs($pcs, $data2->{pcs});
      $symbol_map = MergeSymbols($symbol_map, $data2->{symbols});
    }
  }

  # Subtract base from profile, if specified
  if ($main::opt_base ne '') {
    my $base = ReadProfile($main::prog, $main::opt_base);
    $profile = SubtractProfile($profile, $base->{profile});
    $pcs = AddPcs($pcs, $base->{pcs});
    $symbol_map = MergeSymbols($symbol_map, $base->{symbols});
  }

  # Get total data in profile
  my $total = TotalProfile($profile);

  # Collect symbols
  my $symbols;
  if ($main::use_symbolized_profile) {
    $symbols = FetchSymbols($pcs, $symbol_map);
  } elsif ($main::use_symbol_page) {
    $symbols = FetchSymbols($pcs);
  } else {
    # TODO(csilvers): $libs uses the /proc/self/maps data from profile1,
    # which may differ from the data from subsequent profiles, especially
    # if they were run on different machines.  Use appropriate libs for
    # each pc somehow.
    $symbols = ExtractSymbols($libs, $pcs);
  }

  # Remove uniniteresting stack items
  $profile = RemoveUninterestingFrames($symbols, $profile);

  # Focus?
  if ($main::opt_focus ne '') {
    $profile = FocusProfile($symbols, $profile, $main::opt_focus);
  }

  # Ignore?
  if ($main::opt_ignore ne '') {
    $profile = IgnoreProfile($symbols, $profile, $main::opt_ignore);
  }

  my $calls = ExtractCalls($symbols, $profile);

  # Reduce profiles to required output granularity, and also clean
  # each stack trace so a given entry exists at most once.
  my $reduced = ReduceProfile($symbols, $profile);

  # Get derived profiles
  my $flat = FlatProfile($reduced);
  my $cumulative = CumulativeProfile($reduced);

  # Print
  if (!$main::opt_interactive) {
    if ($main::opt_disasm) {
      PrintDisassembly($libs, $flat, $cumulative, $main::opt_disasm);
    } elsif ($main::opt_list) {
      PrintListing($total, $libs, $flat, $cumulative, $main::opt_list, 0);
    } elsif ($main::opt_text) {
      # Make sure the output is empty when have nothing to report
      # (only matters when --heapcheck is given but we must be
      # compatible with old branches that did not pass --heapcheck always):
      if ($total != 0) {
        printf("Total: %s %s\n", Unparse($total), Units());
      }
      if ($main::opt_stacks) {
        printf("Stacks:\n\n");
        PrintStacksForText($symbols, $profile);
      }
      PrintText($symbols, $flat, $cumulative, -1);
    } elsif ($main::opt_raw) {
      PrintSymbolizedProfile($symbols, $profile, $main::prog);
    } elsif ($main::opt_collapsed) {
      PrintCollapsedStacks($symbols, $profile);
    } elsif ($main::opt_callgrind) {
      PrintCallgrind($calls);
    } else {
      if (PrintDot($main::prog, $symbols, $profile, $flat, $cumulative, $total)) {
        if ($main::opt_gv) {
          RunGV(TempName($main::next_tmpfile, "ps"), "");
        } elsif ($main::opt_evince) {
          RunEvince(TempName($main::next_tmpfile, "pdf"), "");
        } elsif ($main::opt_web) {
          my $tmp = TempName($main::next_tmpfile, "svg");
          RunWeb($tmp);
          # The command we run might hand the file name off
          # to an already running browser instance and then exit.
          # Normally, we'd remove $tmp on exit (right now),
          # but fork a child to remove $tmp a little later, so that the
          # browser has time to load it first.
          delete $main::tempnames{$tmp};
          if (fork() == 0) {
            sleep 5;
            unlink($tmp);
            exit(0);
          }
        }
      } else {
        cleanup();
        exit(1);
      }
    }
  } else {
    InteractiveMode($profile, $symbols, $libs, $total);
  }

  cleanup();
  exit(0);
}

##### Entry Point #####

Main();

# Temporary code to detect if we're running on a Goobuntu system.
# These systems don't have the right stuff installed for the special
# Readline libraries to work, so as a temporary workaround, we default
# to using the normal stdio code, rather than the fancier readline-based
# code
sub ReadlineMightFail {
  if (-e '/lib/libtermcap.so.2') {
    return 0;  # libtermcap exists, so readline should be okay
  } else {
    return 1;
  }
}

sub RunGV {
  my $fname = shift;
  my $bg = shift;       # "" or " &" if we should run in background
  if (!system(ShellEscape(@GV, "--version") . " >$dev_null 2>&1")) {
    # Options using double dash are supported by this gv version.
    # Also, turn on noantialias to better handle bug in gv for
    # postscript files with large dimensions.
    # TODO: Maybe we should not pass the --noantialias flag
    # if the gv version is known to work properly without the flag.
    system(ShellEscape(@GV, "--scale=$main::opt_scale", "--noantialias", $fname)
           . $bg);
  } else {
    # Old gv version - only supports options that use single dash.
    print STDERR ShellEscape(@GV, "-scale", $main::opt_scale) . "\n";
    system(ShellEscape(@GV, "-scale", "$main::opt_scale", $fname) . $bg);
  }
}

sub RunEvince {
  my $fname = shift;
  my $bg = shift;       # "" or " &" if we should run in background
  system(ShellEscape(@EVINCE, $fname) . $bg);
}

sub RunWeb {
  my $fname = shift;
  print STDERR "Loading web page file:///$fname\n";

  if (`uname` =~ /Darwin/) {
    # OS X: open will use standard preference for SVG files.
    system("/usr/bin/open", $fname);
    return;
  }

  if (`uname` =~ /MINGW/) {
    # Windows(MinGW): open will use standard preference for SVG files.
    system("cmd", "/c", "start", $fname);
    return;
  }

  # Some kind of Unix; try generic symlinks, then specific browsers.
  # (Stop once we find one.)
  # Works best if the browser is already running.
  my @alt = (
    "/etc/alternatives/gnome-www-browser",
    "/etc/alternatives/x-www-browser",
    "google-chrome",
    "firefox",
  );
  foreach my $b (@alt) {
    if (system($b, $fname) == 0) {
      return;
    }
  }

  print STDERR "Could not load web browser.\n";
}

sub RunKcachegrind {
  my $fname = shift;
  my $bg = shift;       # "" or " &" if we should run in background
  print STDERR "Starting '@KCACHEGRIND " . $fname . $bg . "'\n";
  system(ShellEscape(@KCACHEGRIND, $fname) . $bg);
}


##### Interactive helper routines #####

sub InteractiveMode {
  $| = 1;  # Make output unbuffered for interactive mode
  my ($orig_profile, $symbols, $libs, $total) = @_;

  print STDERR "Welcome to pprof!  For help, type 'help'.\n";

  # Use ReadLine if it's installed and input comes from a console.
  if ( -t STDIN &&
       !ReadlineMightFail() &&
       defined(eval {require Term::ReadLine}) ) {
    my $term = new Term::ReadLine 'pprof';
    while ( defined ($_ = $term->readline('(pprof) '))) {
      $term->addhistory($_) if /\S/;
      if (!InteractiveCommand($orig_profile, $symbols, $libs, $total, $_)) {
        last;    # exit when we get an interactive command to quit
      }
    }
  } else {       # don't have readline
    while (1) {
      print STDERR "(pprof) ";
      $_ = <STDIN>;
      last if ! defined $_ ;
      s/\r//g;         # turn windows-looking lines into unix-looking lines

      # Save some flags that might be reset by InteractiveCommand()
      my $save_opt_lines = $main::opt_lines;

      if (!InteractiveCommand($orig_profile, $symbols, $libs, $total, $_)) {
        last;    # exit when we get an interactive command to quit
      }

      # Restore flags
      $main::opt_lines = $save_opt_lines;
    }
  }
}

# Takes two args: orig profile, and command to run.
# Returns 1 if we should keep going, or 0 if we were asked to quit
sub InteractiveCommand {
  my($orig_profile, $symbols, $libs, $total, $command) = @_;
  $_ = $command;                # just to make future m//'s easier
  if (!defined($_)) {
    print STDERR "\n";
    return 0;
  }
  if (m/^\s*quit/) {
    return 0;
  }
  if (m/^\s*help/) {
    InteractiveHelpMessage();
    return 1;
  }
  # Clear all the mode options -- mode is controlled by "$command"
  $main::opt_text = 0;
  $main::opt_callgrind = 0;
  $main::opt_disasm = 0;
  $main::opt_list = 0;
  $main::opt_gv = 0;
  $main::opt_evince = 0;
  $main::opt_cum = 0;

  if (m/^\s*(text|top)(\d*)\s*(.*)/) {
    $main::opt_text = 1;

    my $line_limit = ($2 ne "") ? int($2) : 10;

    my $routine;
    my $ignore;
    ($routine, $ignore) = ParseInteractiveArgs($3);

    my $profile = ProcessProfile($total, $orig_profile, $symbols, "", $ignore);
    my $reduced = ReduceProfile($symbols, $profile);

    # Get derived profiles
    my $flat = FlatProfile($reduced);
    my $cumulative = CumulativeProfile($reduced);

    PrintText($symbols, $flat, $cumulative, $line_limit);
    return 1;
  }
  if (m/^\s*callgrind\s*([^ \n]*)/) {
    $main::opt_callgrind = 1;

    # Get derived profiles
    my $calls = ExtractCalls($symbols, $orig_profile);
    my $filename = $1;
    if ( $1 eq '' ) {
      $filename = TempName($main::next_tmpfile, "callgrind");
    }
    PrintCallgrind($calls, $filename);
    if ( $1 eq '' ) {
      RunKcachegrind($filename, " & ");
      $main::next_tmpfile++;
    }

    return 1;
  }
  if (m/^\s*(web)?list\s*(.+)/) {
    my $html = (defined($1) && ($1 eq "web"));
    $main::opt_list = 1;

    my $routine;
    my $ignore;
    ($routine, $ignore) = ParseInteractiveArgs($2);

    my $profile = ProcessProfile($total, $orig_profile, $symbols, "", $ignore);
    my $reduced = ReduceProfile($symbols, $profile);

    # Get derived profiles
    my $flat = FlatProfile($reduced);
    my $cumulative = CumulativeProfile($reduced);

    PrintListing($total, $libs, $flat, $cumulative, $routine, $html);
    return 1;
  }
  if (m/^\s*disasm\s*(.+)/) {
    $main::opt_disasm = 1;

    my $routine;
    my $ignore;
    ($routine, $ignore) = ParseInteractiveArgs($1);

    # Process current profile to account for various settings
    my $profile = ProcessProfile($total, $orig_profile, $symbols, "", $ignore);
    my $reduced = ReduceProfile($symbols, $profile);

    # Get derived profiles
    my $flat = FlatProfile($reduced);
    my $cumulative = CumulativeProfile($reduced);

    PrintDisassembly($libs, $flat, $cumulative, $routine);
    return 1;
  }
  if (m/^\s*(gv|web|evince)\s*(.*)/) {
    $main::opt_gv = 0;
    $main::opt_evince = 0;
    $main::opt_web = 0;
    if ($1 eq "gv") {
      $main::opt_gv = 1;
    } elsif ($1 eq "evince") {
      $main::opt_evince = 1;
    } elsif ($1 eq "web") {
      $main::opt_web = 1;
    }

    my $focus;
    my $ignore;
    ($focus, $ignore) = ParseInteractiveArgs($2);

    # Process current profile to account for various settings
    my $profile = ProcessProfile($total, $orig_profile, $symbols,
                                 $focus, $ignore);
    my $reduced = ReduceProfile($symbols, $profile);

    # Get derived profiles
    my $flat = FlatProfile($reduced);
    my $cumulative = CumulativeProfile($reduced);

    if (PrintDot($main::prog, $symbols, $profile, $flat, $cumulative, $total)) {
      if ($main::opt_gv) {
        RunGV(TempName($main::next_tmpfile, "ps"), " &");
      } elsif ($main::opt_evince) {
        RunEvince(TempName($main::next_tmpfile, "pdf"), " &");
      } elsif ($main::opt_web) {
        RunWeb(TempName($main::next_tmpfile, "svg"));
      }
      $main::next_tmpfile++;
    }
    return 1;
  }
  if (m/^\s*$/) {
    return 1;
  }
  print STDERR "Unknown command: try 'help'.\n";
  return 1;
}


sub ProcessProfile {
  my $total_count = shift;
  my $orig_profile = shift;
  my $symbols = shift;
  my $focus = shift;
  my $ignore = shift;

  # Process current profile to account for various settings
  my $profile = $orig_profile;
  printf("Total: %s %s\n", Unparse($total_count), Units());
  if ($focus ne '') {
    $profile = FocusProfile($symbols, $profile, $focus);
    my $focus_count = TotalProfile($profile);
    printf("After focusing on '%s': %s %s of %s (%0.1f%%)\n",
           $focus,
           Unparse($focus_count), Units(),
           Unparse($total_count), ($focus_count*100.0) / $total_count);
  }
  if ($ignore ne '') {
    $profile = IgnoreProfile($symbols, $profile, $ignore);
    my $ignore_count = TotalProfile($profile);
    printf("After ignoring '%s': %s %s of %s (%0.1f%%)\n",
           $ignore,
           Unparse($ignore_count), Units(),
           Unparse($total_count),
           ($ignore_count*100.0) / $total_count);
  }

  return $profile;
}

sub InteractiveHelpMessage {
  print STDERR <<ENDOFHELP;
Interactive pprof mode
Commands:
  gv
  gv [focus] [-ignore1] [-ignore2]
      Show graphical hierarchical display of current profile.  Without
      any arguments, shows all samples in the profile.  With the optional
      "focus" argument, restricts the samples shown to just those where
      the "focus" regular expression matches a routine name on the stack
      trace.
  web
  web [focus] [-ignore1] [-ignore2]
      Like GV, but displays profile in your web browser instead of using
      Ghostview. Works best if your web browser is already running.
      To change the browser that gets used:
      On Linux, set the /etc/alternatives/gnome-www-browser symlink.
      On OS X, change the Finder association for SVG files.
  list [routine_regexp] [-ignore1] [-ignore2]
      Show source listing of routines whose names match "routine_regexp"
  weblist [routine_regexp] [-ignore1] [-ignore2]
     Displays a source listing of routines whose names match "routine_regexp"
     in a web browser.  You can click on source lines to view the
     corresponding disassembly.
  top [--cum] [-ignore1] [-ignore2]
  top20 [--cum] [-ignore1] [-ignore2]
  top37 [--cum] [-ignore1] [-ignore2]
      Show top lines ordered by flat profile count, or cumulative count
      if --cum is specified.  If a number is present after 'top', the
      top K routines will be shown (defaults to showing the top 10)
  disasm [routine_regexp] [-ignore1] [-ignore2]
      Show disassembly of routines whose names match "routine_regexp",
      annotated with sample counts.
  callgrind
  callgrind [filename]
      Generates callgrind file. If no filename is given, kcachegrind is called.
  help - This listing
  quit or ^D - End pprof
For commands that accept optional -ignore tags, samples where any routine in
the stack trace matches the regular expression in any of the -ignore
parameters will be ignored.
Further pprof details are available at this location (or one similar):
 /usr/doc/gperftools-$PPROF_VERSION/cpu_profiler.html
 /usr/doc/gperftools-$PPROF_VERSION/heap_profiler.html
ENDOFHELP
}
sub ParseInteractiveArgs {
  my $args = shift;
  my $focus = "";
  my $ignore = "";
  my @x = split(/ +/, $args);
  foreach $a (@x) {
    if ($a =~ m/^(--|-)lines$/) {
      $main::opt_lines = 1;
    } elsif ($a =~ m/^(--|-)cum$/) {
      $main::opt_cum = 1;
    } elsif ($a =~ m/^-(.*)/) {
      $ignore .= (($ignore ne "") ? "|" : "" ) . $1;
    } else {
      $focus .= (($focus ne "") ? "|" : "" ) . $a;
    }
  }
  if ($ignore ne "") {
    print STDERR "Ignoring samples in call stacks that match '$ignore'\n";
  }
  return ($focus, $ignore);
}

##### Output code #####

sub TempName {
  my $fnum = shift;
  my $ext = shift;
  my $file = "$main::tmpfile_ps.$fnum.$ext";
  $main::tempnames{$file} = 1;
  return $file;
}

# Print profile data in packed binary format (64-bit) to standard out
sub PrintProfileData {
  my $profile = shift;
  my $big_endian = pack("L", 1) eq pack("N", 1);
  # print header (64-bit style)
  # (zero) (header-size) (version) (sample-period) (zero)
  if ($big_endian) {
    print pack('L*', 0, 0, 0, 3, 0, 0, 0, 1, 0, 0);
  }
  else {
    print pack('L*', 0, 0, 3, 0, 0, 0, 1, 0, 0, 0);
  }

  foreach my $k (keys(%{$profile})) {
    my $count = $profile->{$k};
    my @addrs = split(/\n/, $k);
    if ($#addrs >= 0) {
      my $depth = $#addrs + 1;
      # int(foo / 2**32) is the only reliable way to get rid of bottom
      # 32 bits on both 32- and 64-bit systems.
      if ($big_endian) {
        print pack('L*', int($count / 2**32), $count & 0xFFFFFFFF);
        print pack('L*', int($depth / 2**32), $depth & 0xFFFFFFFF);
      }
      else {
        print pack('L*', $count & 0xFFFFFFFF, int($count / 2**32));
        print pack('L*', $depth & 0xFFFFFFFF, int($depth / 2**32));
      }

      foreach my $full_addr (@addrs) {
        my $addr = $full_addr;
        $addr =~ s/0x0*//;  # strip off leading 0x, zeroes
        if (length($addr) > 16) {
          print STDERR "Invalid address in profile: $full_addr\n";
          next;
        }
        my $low_addr = substr($addr, -8);       # get last 8 hex chars
        my $high_addr = substr($addr, -16, 8);  # get up to 8 more hex chars
        if ($big_endian) {
          print pack('L*', hex('0x' . $high_addr), hex('0x' . $low_addr));
        }
        else {
          print pack('L*', hex('0x' . $low_addr), hex('0x' . $high_addr));
        }
      }
    }
  }
}

# Print symbols and profile data
sub PrintSymbolizedProfile {
  my $symbols = shift;
  my $profile = shift;
  my $prog = shift;

  $SYMBOL_PAGE =~ m,[^/]+$,;    # matches everything after the last slash
  my $symbol_marker = $&;

  print '--- ', $symbol_marker, "\n";
  if (defined($prog)) {
    print 'binary=', $prog, "\n";
  }
  while (my ($pc, $name) = each(%{$symbols})) {
    my $sep = ' ';
    print '0x', $pc;
    # We have a list of function names, which include the inlined
    # calls.  They are separated (and terminated) by --, which is
    # illegal in function names.
    for (my $j = 2; $j <= $#{$name}; $j += 3) {
      print $sep, $name->[$j];
      $sep = '--';
    }
    print "\n";
  }
  print '---', "\n";

  $PROFILE_PAGE =~ m,[^/]+$,;    # matches everything after the last slash
  my $profile_marker = $&;
  print '--- ', $profile_marker, "\n";
  if (defined($main::collected_profile)) {
    # if used with remote fetch, simply dump the collected profile to output.
    open(SRC, "<$main::collected_profile");
    while (<SRC>) {
      print $_;
    }
    close(SRC);
  } else {
    # dump a cpu-format profile to standard out
    PrintProfileData($profile);
  }
}

# Print text output
sub PrintText {
  my $symbols = shift;
  my $flat = shift;
  my $cumulative = shift;
  my $line_limit = shift;

  if ($main::opt_stacks && @stackTraces) {
      foreach (sort { (split " ", $b)[1] <=> (split " ", $a)[1]; } @stackTraces) {
	  print "$_\n" if $main::opt_debug;
	  my ($n1, $s1, $n2, $s2, @addrs) = split;
	  print "Leak of $s1 bytes in $n1 objects allocated from:\n";
	  foreach my $pcstr (@addrs) {
	      $pcstr =~ s/^0x//;
	      my $sym;
	      if (! defined $symbols->{$pcstr}) {
		  $sym = "unknown";
	      } else {
		  $sym = "$symbols->{$pcstr}[0] $symbols->{$pcstr}[1]";
	      }
	      print "\t@ $pcstr $sym\n";
	  }
      }
      print "\n";
  }

  my $total = TotalProfile($flat);

  # Which profile to sort by?
  my $s = $main::opt_cum ? $cumulative : $flat;

  my $running_sum = 0;
  my $lines = 0;
  foreach my $k (sort { GetEntry($s, $b) <=> GetEntry($s, $a) || $a cmp $b }
                 keys(%{$cumulative})) {
    my $f = GetEntry($flat, $k);
    my $c = GetEntry($cumulative, $k);
    $running_sum += $f;

    my $sym = $k;
    if (exists($symbols->{$k})) {
      $sym = $symbols->{$k}->[0] . " " . $symbols->{$k}->[1];
      if ($main::opt_addresses) {
        $sym = $k . " " . $sym;
      }
    }

    if ($f != 0 || $c != 0) {
      printf("%8s %6s %6s %8s %6s %s\n",
             Unparse($f),
             Percent($f, $total),
             Percent($running_sum, $total),
             Unparse($c),
             Percent($c, $total),
             $sym);
    }
    $lines++;
    last if ($line_limit >= 0 && $lines >= $line_limit);
  }
}

# Callgrind format has a compression for repeated function and file
# names.  You show the name the first time, and just use its number
# subsequently.  This can cut down the file to about a third or a
# quarter of its uncompressed size.  $key and $val are the key/value
# pair that would normally be printed by callgrind; $map is a map from
# value to number.
sub CompressedCGName {
  my($key, $val, $map) = @_;
  my $idx = $map->{$val};
  # For very short keys, providing an index hurts rather than helps.
  if (length($val) <= 3) {
    return "$key=$val\n";
  } elsif (defined($idx)) {
    return "$key=($idx)\n";
  } else {
    # scalar(keys $map) gives the number of items in the map.
    $idx = scalar(keys(%{$map})) + 1;
    $map->{$val} = $idx;
    return "$key=($idx) $val\n";
  }
}

# Print the call graph in a way that's suiteable for callgrind.
sub PrintCallgrind {
  my $calls = shift;
  my $filename;
  my %filename_to_index_map;
  my %fnname_to_index_map;

  if ($main::opt_interactive) {
    $filename = shift;
    print STDERR "Writing callgrind file to '$filename'.\n"
  } else {
    $filename = "&STDOUT";
  }
  open(CG, ">$filename");
  print CG ("events: Hits\n\n");
  foreach my $call ( map { $_->[0] }
                     sort { $a->[1] cmp $b ->[1] ||
                            $a->[2] <=> $b->[2] }
                     map { /([^:]+):(\d+):([^ ]+)( -> ([^:]+):(\d+):(.+))?/;
                           [$_, $1, $2] }
                     keys %$calls ) {
    my $count = int($calls->{$call});
    $call =~ /([^:]+):(\d+):([^ ]+)( -> ([^:]+):(\d+):(.+))?/;
    my ( $caller_file, $caller_line, $caller_function,
         $callee_file, $callee_line, $callee_function ) =
       ( $1, $2, $3, $5, $6, $7 );

    # TODO(csilvers): for better compression, collect all the
    # caller/callee_files and functions first, before printing
    # anything, and only compress those referenced more than once.
    print CG CompressedCGName("fl", $caller_file, \%filename_to_index_map);
    print CG CompressedCGName("fn", $caller_function, \%fnname_to_index_map);
    if (defined $6) {
      print CG CompressedCGName("cfl", $callee_file, \%filename_to_index_map);
      print CG CompressedCGName("cfn", $callee_function, \%fnname_to_index_map);
      print CG ("calls=$count $callee_line\n");
    }
    print CG ("$caller_line $count\n\n");
  }
}

# Print disassembly for all all routines that match $main::opt_disasm
sub PrintDisassembly {
  my $libs = shift;
  my $flat = shift;
  my $cumulative = shift;
  my $disasm_opts = shift;

  my $total = TotalProfile($flat);

  foreach my $lib (@{$libs}) {
    my $symbol_table = GetProcedureBoundaries($lib->[0], $disasm_opts);
    my $offset = AddressSub($lib->[1], $lib->[3]);
    foreach my $routine (sort ByName keys(%{$symbol_table})) {
      my $start_addr = $symbol_table->{$routine}->[0];
      my $end_addr = $symbol_table->{$routine}->[1];
      # See if there are any samples in this routine
      my $length = hex(AddressSub($end_addr, $start_addr));
      my $addr = AddressAdd($start_addr, $offset);
      for (my $i = 0; $i < $length; $i++) {
        if (defined($cumulative->{$addr})) {
          PrintDisassembledFunction($lib->[0], $offset,
                                    $routine, $flat, $cumulative,
                                    $start_addr, $end_addr, $total);
          last;
        }
        $addr = AddressInc($addr);
      }
    }
  }
}

# Return reference to array of tuples of the form:
#       [start_address, filename, linenumber, instruction, limit_address]
# E.g.,
#       ["0x806c43d", "/foo/bar.cc", 131, "ret", "0x806c440"]
sub Disassemble {
  my $prog = shift;
  my $offset = shift;
  my $start_addr = shift;
  my $end_addr = shift;

  my $objdump = $obj_tool_map{"objdump"};
  my $cmd = ShellEscape($objdump, "-C", "-d", "-l", "--no-show-raw-insn",
                        "--start-address=0x$start_addr",
                        "--stop-address=0x$end_addr", $prog);
  open(OBJDUMP, "$cmd |") || error("$cmd: $!\n");
  my @result = ();
  my $filename = "";
  my $linenumber = -1;
  my $last = ["", "", "", ""];
  while (<OBJDUMP>) {
    s/\r//g;         # turn windows-looking lines into unix-looking lines
    chop;
    if (m|\s*([^:\s]+):(\d+)\s*$|) {
      # Location line of the form:
      #   <filename>:<linenumber>
      $filename = $1;
      $linenumber = $2;
    } elsif (m/^ +([0-9a-f]+):\s*(.*)/) {
      # Disassembly line -- zero-extend address to full length
      my $addr = HexExtend($1);
      my $k = AddressAdd($addr, $offset);
      $last->[4] = $k;   # Store ending address for previous instruction
      $last = [$k, $filename, $linenumber, $2, $end_addr];
      push(@result, $last);
    }
  }
  close(OBJDUMP);
  return @result;
}

# The input file should contain lines of the form /proc/maps-like
# output (same format as expected from the profiles) or that looks
# like hex addresses (like "0xDEADBEEF").  We will parse all
# /proc/maps output, and for all the hex addresses, we will output
# "short" symbol names, one per line, in the same order as the input.
sub PrintSymbols {
  my $maps_and_symbols_file = shift;

  # ParseLibraries expects pcs to be in a set.  Fine by us...
  my @pclist = ();   # pcs in sorted order
  my $pcs = {};
  my $map = "";
  foreach my $line (<$maps_and_symbols_file>) {
    $line =~ s/\r//g;    # turn windows-looking lines into unix-looking lines
    if ($line =~ /\b(0x[0-9a-f]+)\b/i) {
      push(@pclist, HexExtend($1));
      $pcs->{$pclist[-1]} = 1;
    } else {
      $map .= $line;
    }
  }

  my $libs = ParseLibraries($main::prog, $map, $pcs);
  my $symbols = ExtractSymbols($libs, $pcs);

  foreach my $pc (@pclist) {
    # ->[0] is the shortname, ->[2] is the full name
    print(($symbols->{$pc}->[0] || "??") . "\n");
  }
}


# For sorting functions by name
sub ByName {
  return ShortFunctionName($a) cmp ShortFunctionName($b);
}

# Print source-listing for all all routines that match $list_opts
sub PrintListing {
  my $total = shift;
  my $libs = shift;
  my $flat = shift;
  my $cumulative = shift;
  my $list_opts = shift;
  my $html = shift;

  my $output = \*STDOUT;
  my $fname = "";

  if ($html) {
    # Arrange to write the output to a temporary file
    $fname = TempName($main::next_tmpfile, "html");
    $main::next_tmpfile++;
    if (!open(TEMP, ">$fname")) {
      print STDERR "$fname: $!\n";
      return;
    }
    $output = \*TEMP;
    print $output HtmlListingHeader();
    printf $output ("<div class=\"legend\">%s<br>Total: %s %s</div>\n",
                    $main::prog, Unparse($total), Units());
  }

  my $listed = 0;
  foreach my $lib (@{$libs}) {
    my $symbol_table = GetProcedureBoundaries($lib->[0], $list_opts);
    my $offset = AddressSub($lib->[1], $lib->[3]);
    foreach my $routine (sort ByName keys(%{$symbol_table})) {
      # Print if there are any samples in this routine
      my $start_addr = $symbol_table->{$routine}->[0];
      my $end_addr = $symbol_table->{$routine}->[1];
      my $length = hex(AddressSub($end_addr, $start_addr));
      my $addr = AddressAdd($start_addr, $offset);
      for (my $i = 0; $i < $length; $i++) {
        if (defined($cumulative->{$addr})) {
          $listed += PrintSource(
            $lib->[0], $offset,
            $routine, $flat, $cumulative,
            $start_addr, $end_addr,
            $html,
            $output);
          last;
        }
        $addr = AddressInc($addr);
      }
    }
  }

  if ($html) {
    if ($listed > 0) {
      print $output HtmlListingFooter();
      close($output);
      RunWeb($fname);
    } else {
      close($output);
      unlink($fname);
    }
  }
}

sub HtmlListingHeader {
  return <<'EOF';
<DOCTYPE html>
<html>
<head>
<title>Pprof listing</title>
<style type="text/css">
body {
  font-family: sans-serif;
}
h1 {
  font-size: 1.5em;
  margin-bottom: 4px;
}
.legend {
  font-size: 1.25em;
}
.line {
  color: #aaaaaa;
}
.nop {
  color: #aaaaaa;
}
.unimportant {
  color: #cccccc;
}
.disasmloc {
  color: #000000;
}
.deadsrc {
  cursor: pointer;
}
.deadsrc:hover {
  background-color: #eeeeee;
}
.livesrc {
  color: #0000ff;
  cursor: pointer;
}
.livesrc:hover {
  background-color: #eeeeee;
}
.asm {
  color: #008800;
  display: none;
}
</style>
<script type="text/javascript">
function pprof_toggle_asm(e) {
  var target;
  if (!e) e = window.event;
  if (e.target) target = e.target;
  else if (e.srcElement) target = e.srcElement;
  if (target) {
    var asm = target.nextSibling;
    if (asm && asm.className == "asm") {
      asm.style.display = (asm.style.display == "block" ? "" : "block");
      e.preventDefault();
      return false;
    }
  }
}
</script>
</head>
<body>
EOF
}

sub HtmlListingFooter {
  return <<'EOF';
</body>
</html>
EOF
}

sub HtmlEscape {
  my $text = shift;
  $text =~ s/&/&amp;/g;
  $text =~ s/</&lt;/g;
  $text =~ s/>/&gt;/g;
  return $text;
}

# Returns the indentation of the line, if it has any non-whitespace
# characters.  Otherwise, returns -1.
sub Indentation {
  my $line = shift;
  if (m/^(\s*)\S/) {
    return length($1);
  } else {
    return -1;
  }
}

# If the symbol table contains inlining info, Disassemble() may tag an
# instruction with a location inside an inlined function.  But for
# source listings, we prefer to use the location in the function we
# are listing.  So use MapToSymbols() to fetch full location
# information for each instruction and then pick out the first
# location from a location list (location list contains callers before
# callees in case of inlining).
#
# After this routine has run, each entry in $instructions contains:
#   [0] start address
#   [1] filename for function we are listing
#   [2] line number for function we are listing
#   [3] disassembly
#   [4] limit address
#   [5] most specific filename (may be different from [1] due to inlining)
#   [6] most specific line number (may be different from [2] due to inlining)
sub GetTopLevelLineNumbers {
  my ($lib, $offset, $instructions) = @_;
  my $pcs = [];
  for (my $i = 0; $i <= $#{$instructions}; $i++) {
    push(@{$pcs}, $instructions->[$i]->[0]);
  }
  my $symbols = {};
  MapToSymbols($lib, $offset, $pcs, $symbols);
  for (my $i = 0; $i <= $#{$instructions}; $i++) {
    my $e = $instructions->[$i];
    push(@{$e}, $e->[1]);
    push(@{$e}, $e->[2]);
    my $addr = $e->[0];
    my $sym = $symbols->{$addr};
    if (defined($sym)) {
      if ($#{$sym} >= 2 && $sym->[1] =~ m/^(.*):(\d+)$/) {
        $e->[1] = $1;  # File name
        $e->[2] = $2;  # Line number
      }
    }
  }
}

# Print source-listing for one routine
sub PrintSource {
  my $prog = shift;
  my $offset = shift;
  my $routine = shift;
  my $flat = shift;
  my $cumulative = shift;
  my $start_addr = shift;
  my $end_addr = shift;
  my $html = shift;
  my $output = shift;

  # Disassemble all instructions (just to get line numbers)
  my @instructions = Disassemble($prog, $offset, $start_addr, $end_addr);
  GetTopLevelLineNumbers($prog, $offset, \@instructions);

  # Hack 1: assume that the first source file encountered in the
  # disassembly contains the routine
  my $filename = undef;
  for (my $i = 0; $i <= $#instructions; $i++) {
    if ($instructions[$i]->[2] >= 0) {
      $filename = $instructions[$i]->[1];
      last;
    }
  }
  if (!defined($filename)) {
    print STDERR "no filename found in $routine\n";
    return 0;
  }

  # Hack 2: assume that the largest line number from $filename is the
  # end of the procedure.  This is typically safe since if P1 contains
  # an inlined call to P2, then P2 usually occurs earlier in the
  # source file.  If this does not work, we might have to compute a
  # density profile or just print all regions we find.
  my $lastline = 0;
  for (my $i = 0; $i <= $#instructions; $i++) {
    my $f = $instructions[$i]->[1];
    my $l = $instructions[$i]->[2];
    if (($f eq $filename) && ($l > $lastline)) {
      $lastline = $l;
    }
  }

  # Hack 3: assume the first source location from "filename" is the start of
  # the source code.
  my $firstline = 1;
  for (my $i = 0; $i <= $#instructions; $i++) {
    if ($instructions[$i]->[1] eq $filename) {
      $firstline = $instructions[$i]->[2];
      last;
    }
  }

  # Hack 4: Extend last line forward until its indentation is less than
  # the indentation we saw on $firstline
  my $oldlastline = $lastline;
  {
    if (!open(FILE, "<$filename")) {
      print STDERR "$filename: $!\n";
      return 0;
    }
    my $l = 0;
    my $first_indentation = -1;
    while (<FILE>) {
      s/\r//g;         # turn windows-looking lines into unix-looking lines
      $l++;
      my $indent = Indentation($_);
      if ($l >= $firstline) {
        if ($first_indentation < 0 && $indent >= 0) {
          $first_indentation = $indent;
          last if ($first_indentation == 0);
        }
      }
      if ($l >= $lastline && $indent >= 0) {
        if ($indent >= $first_indentation) {
          $lastline = $l+1;
        } else {
          last;
        }
      }
    }
    close(FILE);
  }

  # Assign all samples to the range $firstline,$lastline,
  # Hack 4: If an instruction does not occur in the range, its samples
  # are moved to the next instruction that occurs in the range.
  my $samples1 = {};        # Map from line number to flat count
  my $samples2 = {};        # Map from line number to cumulative count
  my $running1 = 0;         # Unassigned flat counts
  my $running2 = 0;         # Unassigned cumulative counts
  my $total1 = 0;           # Total flat counts
  my $total2 = 0;           # Total cumulative counts
  my %disasm = ();          # Map from line number to disassembly
  my $running_disasm = "";  # Unassigned disassembly
  my $skip_marker = "---\n";
  if ($html) {
    $skip_marker = "";
    for (my $l = $firstline; $l <= $lastline; $l++) {
      $disasm{$l} = "";
    }
  }
  my $last_dis_filename = '';
  my $last_dis_linenum = -1;
  my $last_touched_line = -1;  # To detect gaps in disassembly for a line
  foreach my $e (@instructions) {
    # Add up counts for all address that fall inside this instruction
    my $c1 = 0;
    my $c2 = 0;
    for (my $a = $e->[0]; $a lt $e->[4]; $a = AddressInc($a)) {
      $c1 += GetEntry($flat, $a);
      $c2 += GetEntry($cumulative, $a);
    }

    if ($html) {
      my $dis = sprintf("      %6s %6s \t\t%8s: %s ",
                        HtmlPrintNumber($c1),
                        HtmlPrintNumber($c2),
                        UnparseAddress($offset, $e->[0]),
                        CleanDisassembly($e->[3]));
      
      # Append the most specific source line associated with this instruction
      if (length($dis) < 80) { $dis .= (' ' x (80 - length($dis))) };
      $dis = HtmlEscape($dis);
      my $f = $e->[5];
      my $l = $e->[6];
      if ($f ne $last_dis_filename) {
        $dis .= sprintf("<span class=disasmloc>%s:%d</span>", 
                        HtmlEscape(CleanFileName($f)), $l);
      } elsif ($l ne $last_dis_linenum) {
        # De-emphasize the unchanged file name portion
        $dis .= sprintf("<span class=unimportant>%s</span>" .
                        "<span class=disasmloc>:%d</span>", 
                        HtmlEscape(CleanFileName($f)), $l);
      } else {
        # De-emphasize the entire location
        $dis .= sprintf("<span class=unimportant>%s:%d</span>", 
                        HtmlEscape(CleanFileName($f)), $l);
      }
      $last_dis_filename = $f;
      $last_dis_linenum = $l;
      $running_disasm .= $dis;
      $running_disasm .= "\n";
    }

    $running1 += $c1;
    $running2 += $c2;
    $total1 += $c1;
    $total2 += $c2;
    my $file = $e->[1];
    my $line = $e->[2];
    if (($file eq $filename) &&
        ($line >= $firstline) &&
        ($line <= $lastline)) {
      # Assign all accumulated samples to this line
      AddEntry($samples1, $line, $running1);
      AddEntry($samples2, $line, $running2);
      $running1 = 0;
      $running2 = 0;
      if ($html) {
        if ($line != $last_touched_line && $disasm{$line} ne '') {
          $disasm{$line} .= "\n";
        }
        $disasm{$line} .= $running_disasm;
        $running_disasm = '';
        $last_touched_line = $line;
      }
    }
  }

  # Assign any leftover samples to $lastline
  AddEntry($samples1, $lastline, $running1);
  AddEntry($samples2, $lastline, $running2);
  if ($html) {
    if ($lastline != $last_touched_line && $disasm{$lastline} ne '') {
      $disasm{$lastline} .= "\n";
    }
    $disasm{$lastline} .= $running_disasm;
  }

  if ($html) {
    printf $output (
      "<h1>%s</h1>%s\n<pre onClick=\"pprof_toggle_asm()\">\n" .
      "Total:%6s %6s (flat / cumulative %s)\n",
      HtmlEscape(ShortFunctionName($routine)),
      HtmlEscape(CleanFileName($filename)),
      Unparse($total1),
      Unparse($total2),
      Units());
  } else {
    printf $output (
      "ROUTINE ====================== %s in %s\n" .
      "%6s %6s Total %s (flat / cumulative)\n",
      ShortFunctionName($routine),
      CleanFileName($filename),
      Unparse($total1),
      Unparse($total2),
      Units());
  }
  if (!open(FILE, "<$filename")) {
    print STDERR "$filename: $!\n";
    return 0;
  }
  my $l = 0;
  while (<FILE>) {
    s/\r//g;         # turn windows-looking lines into unix-looking lines
    $l++;
    if ($l >= $firstline - 5 &&
        (($l <= $oldlastline + 5) || ($l <= $lastline))) {
      chop;
      my $text = $_;
      if ($l == $firstline) { print $output $skip_marker; }
      my $n1 = GetEntry($samples1, $l);
      my $n2 = GetEntry($samples2, $l);
      if ($html) {
        # Emit a span that has one of the following classes:
        #    livesrc -- has samples
        #    deadsrc -- has disassembly, but with no samples
        #    nop     -- has no matching disasembly
        # Also emit an optional span containing disassembly.
        my $dis = $disasm{$l};
        my $asm = "";
        if (defined($dis) && $dis ne '') {
          $asm = "<span class=\"asm\">" . $dis . "</span>";
        }
        my $source_class = (($n1 + $n2 > 0) 
                            ? "livesrc" 
                            : (($asm ne "") ? "deadsrc" : "nop"));
        printf $output (
          "<span class=\"line\">%5d</span> " .
          "<span class=\"%s\">%6s %6s %s</span>%s\n",
          $l, $source_class,
          HtmlPrintNumber($n1),
          HtmlPrintNumber($n2),
          HtmlEscape($text),
          $asm);
      } else {
        printf $output(
          "%6s %6s %4d: %s\n",
          UnparseAlt($n1),
          UnparseAlt($n2),
          $l,
          $text);
      }
      if ($l == $lastline)  { print $output $skip_marker; }
    };
  }
  close(FILE);
  if ($html) {
    print $output "</pre>\n";
  }
  return 1;
}

# Return the source line for the specified file/linenumber.
# Returns undef if not found.
sub SourceLine {
  my $file = shift;
  my $line = shift;

  # Look in cache
  if (!defined($main::source_cache{$file})) {
    if (100 < scalar keys(%main::source_cache)) {
      # Clear the cache when it gets too big
      $main::source_cache = ();
    }

    # Read all lines from the file
    if (!open(FILE, "<$file")) {
      print STDERR "$file: $!\n";
      $main::source_cache{$file} = [];  # Cache the negative result
      return undef;
    }
    my $lines = [];
    push(@{$lines}, "");        # So we can use 1-based line numbers as indices
    while (<FILE>) {
      push(@{$lines}, $_);
    }
    close(FILE);

    # Save the lines in the cache
    $main::source_cache{$file} = $lines;
  }

  my $lines = $main::source_cache{$file};
  if (($line < 0) || ($line > $#{$lines})) {
    return undef;
  } else {
    return $lines->[$line];
  }
}

# Print disassembly for one routine with interspersed source if available
sub PrintDisassembledFunction {
  my $prog = shift;
  my $offset = shift;
  my $routine = shift;
  my $flat = shift;
  my $cumulative = shift;
  my $start_addr = shift;
  my $end_addr = shift;
  my $total = shift;

  # Disassemble all instructions
  my @instructions = Disassemble($prog, $offset, $start_addr, $end_addr);

  # Make array of counts per instruction
  my @flat_count = ();
  my @cum_count = ();
  my $flat_total = 0;
  my $cum_total = 0;
  foreach my $e (@instructions) {
    # Add up counts for all address that fall inside this instruction
    my $c1 = 0;
    my $c2 = 0;
    for (my $a = $e->[0]; $a lt $e->[4]; $a = AddressInc($a)) {
      $c1 += GetEntry($flat, $a);
      $c2 += GetEntry($cumulative, $a);
    }
    push(@flat_count, $c1);
    push(@cum_count, $c2);
    $flat_total += $c1;
    $cum_total += $c2;
  }

  # Print header with total counts
  printf("ROUTINE ====================== %s\n" .
         "%6s %6s %s (flat, cumulative) %.1f%% of total\n",
         ShortFunctionName($routine),
         Unparse($flat_total),
         Unparse($cum_total),
         Units(),
         ($cum_total * 100.0) / $total);

  # Process instructions in order
  my $current_file = "";
  for (my $i = 0; $i <= $#instructions; ) {
    my $e = $instructions[$i];

    # Print the new file name whenever we switch files
    if ($e->[1] ne $current_file) {
      $current_file = $e->[1];
      my $fname = $current_file;
      $fname =~ s|^\./||;   # Trim leading "./"

      # Shorten long file names
      if (length($fname) >= 58) {
        $fname = "..." . substr($fname, -55);
      }
      printf("-------------------- %s\n", $fname);
    }

    # TODO: Compute range of lines to print together to deal with
    # small reorderings.
    my $first_line = $e->[2];
    my $last_line = $first_line;
    my %flat_sum = ();
    my %cum_sum = ();
    for (my $l = $first_line; $l <= $last_line; $l++) {
      $flat_sum{$l} = 0;
      $cum_sum{$l} = 0;
    }

    # Find run of instructions for this range of source lines
    my $first_inst = $i;
    while (($i <= $#instructions) &&
           ($instructions[$i]->[2] >= $first_line) &&
           ($instructions[$i]->[2] <= $last_line)) {
      $e = $instructions[$i];
      $flat_sum{$e->[2]} += $flat_count[$i];
      $cum_sum{$e->[2]} += $cum_count[$i];
      $i++;
    }
    my $last_inst = $i - 1;

    # Print source lines
    for (my $l = $first_line; $l <= $last_line; $l++) {
      my $line = SourceLine($current_file, $l);
      if (!defined($line)) {
        $line = "?\n";
        next;
      } else {
        $line =~ s/^\s+//;
      }
      printf("%6s %6s %5d: %s",
             UnparseAlt($flat_sum{$l}),
             UnparseAlt($cum_sum{$l}),
             $l,
             $line);
    }

    # Print disassembly
    for (my $x = $first_inst; $x <= $last_inst; $x++) {
      my $e = $instructions[$x];
      printf("%6s %6s    %8s: %6s\n",
             UnparseAlt($flat_count[$x]),
             UnparseAlt($cum_count[$x]),
             UnparseAddress($offset, $e->[0]),
             CleanDisassembly($e->[3]));
    }
  }
}

# Print DOT graph
sub PrintDot {
  my $prog = shift;
  my $symbols = shift;
  my $raw = shift;
  my $flat = shift;
  my $cumulative = shift;
  my $overall_total = shift;

  # Get total
  my $local_total = TotalProfile($flat);
  my $nodelimit = int($main::opt_nodefraction * $local_total);
  my $edgelimit = int($main::opt_edgefraction * $local_total);
  my $nodecount = $main::opt_nodecount;

  # Find nodes to include
  my @list = (sort { abs(GetEntry($cumulative, $b)) <=>
                     abs(GetEntry($cumulative, $a))
                     || $a cmp $b }
              keys(%{$cumulative}));
  my $last = $nodecount - 1;
  if ($last > $#list) {
    $last = $#list;
  }
  while (($last >= 0) &&
         (abs(GetEntry($cumulative, $list[$last])) <= $nodelimit)) {
    $last--;
  }
  if ($last < 0) {
    print STDERR "No nodes to print\n";
    return 0;
  }

  if ($nodelimit > 0 || $edgelimit > 0) {
    printf STDERR ("Dropping nodes with <= %s %s; edges with <= %s abs(%s)\n",
                   Unparse($nodelimit), Units(),
                   Unparse($edgelimit), Units());
  }

  # Open DOT output file
  my $output;
  my $escaped_dot = ShellEscape(@DOT);
  my $escaped_ps2pdf = ShellEscape(@PS2PDF);
  if ($main::opt_gv) {
    my $escaped_outfile = ShellEscape(TempName($main::next_tmpfile, "ps"));
    $output = "| $escaped_dot -Tps2 >$escaped_outfile";
  } elsif ($main::opt_evince) {
    my $escaped_outfile = ShellEscape(TempName($main::next_tmpfile, "pdf"));
    $output = "| $escaped_dot -Tps2 | $escaped_ps2pdf - $escaped_outfile";
  } elsif ($main::opt_ps) {
    $output = "| $escaped_dot -Tps2";
  } elsif ($main::opt_pdf) {
    $output = "| $escaped_dot -Tps2 | $escaped_ps2pdf - -";
  } elsif ($main::opt_web || $main::opt_svg) {
    # We need to post-process the SVG, so write to a temporary file always.
    my $escaped_outfile = ShellEscape(TempName($main::next_tmpfile, "svg"));
    $output = "| $escaped_dot -Tsvg >$escaped_outfile";
  } elsif ($main::opt_gif) {
    $output = "| $escaped_dot -Tgif";
  } else {
    $output = ">&STDOUT";
  }
  open(DOT, $output) || error("$output: $!\n");

  # Title
  printf DOT ("digraph \"%s; %s %s\" {\n",
              $prog,
              Unparse($overall_total),
              Units());
  if ($main::opt_pdf) {
    # The output is more printable if we set the page size for dot.
    printf DOT ("size=\"8,11\"\n");
  }
  printf DOT ("node [width=0.375,height=0.25];\n");

  # Print legend
  printf DOT ("Legend [shape=box,fontsize=24,shape=plaintext," .
              "label=\"%s\\l%s\\l%s\\l%s\\l%s\\l\"];\n",
              $prog,
              sprintf("Total %s: %s", Units(), Unparse($overall_total)),
              sprintf("Focusing on: %s", Unparse($local_total)),
              sprintf("Dropped nodes with <= %s abs(%s)",
                      Unparse($nodelimit), Units()),
              sprintf("Dropped edges with <= %s %s",
                      Unparse($edgelimit), Units())
              );

  # Print nodes
  my %node = ();
  my $nextnode = 1;
  foreach my $a (@list[0..$last]) {
    # Pick font size
    my $f = GetEntry($flat, $a);
    my $c = GetEntry($cumulative, $a);

    my $fs = 8;
    if ($local_total > 0) {
      $fs = 8 + (50.0 * sqrt(abs($f * 1.0 / $local_total)));
    }

    $node{$a} = $nextnode++;
    my $sym = $a;
    $sym =~ s/\s+/\\n/g;
    $sym =~ s/::/\\n/g;

    # Extra cumulative info to print for non-leaves
    my $extra = "";
    if ($f != $c) {
      $extra = sprintf("\\rof %s (%s)",
                       Unparse($c),
                       Percent($c, $local_total));
    }
    my $style = "";
    if ($main::opt_heapcheck) {
      if ($f > 0) {
        # make leak-causing nodes more visible (add a background)
        $style = ",style=filled,fillcolor=gray"
      } elsif ($f < 0) {
        # make anti-leak-causing nodes (which almost never occur)
        # stand out as well (triple border)
        $style = ",peripheries=3"
      }
    }

    printf DOT ("N%d [label=\"%s\\n%s (%s)%s\\r" .
                "\",shape=box,fontsize=%.1f%s];\n",
                $node{$a},
                $sym,
                Unparse($f),
                Percent($f, $local_total),
                $extra,
                $fs,
                $style,
               );
  }

  # Get edges and counts per edge
  my %edge = ();
  my $n;
  my $fullname_to_shortname_map = {};
  FillFullnameToShortnameMap($symbols, $fullname_to_shortname_map);
  foreach my $k (keys(%{$raw})) {
    # TODO: omit low %age edges
    $n = $raw->{$k};
    my @translated = TranslateStack($symbols, $fullname_to_shortname_map, $k);
    for (my $i = 1; $i <= $#translated; $i++) {
      my $src = $translated[$i];
      my $dst = $translated[$i-1];
      #next if ($src eq $dst);  # Avoid self-edges?
      if (exists($node{$src}) && exists($node{$dst})) {
        my $edge_label = "$src\001$dst";
        if (!exists($edge{$edge_label})) {
          $edge{$edge_label} = 0;
        }
        $edge{$edge_label} += $n;
      }
    }
  }

  # Print edges (process in order of decreasing counts)
  my %indegree = ();   # Number of incoming edges added per node so far
  my %outdegree = ();  # Number of outgoing edges added per node so far
  foreach my $e (sort { $edge{$b} <=> $edge{$a} } keys(%edge)) {
    my @x = split(/\001/, $e);
    $n = $edge{$e};

    # Initialize degree of kept incoming and outgoing edges if necessary
    my $src = $x[0];
    my $dst = $x[1];
    if (!exists($outdegree{$src})) { $outdegree{$src} = 0; }
    if (!exists($indegree{$dst})) { $indegree{$dst} = 0; }

    my $keep;
    if ($indegree{$dst} == 0) {
      # Keep edge if needed for reachability
      $keep = 1;
    } elsif (abs($n) <= $edgelimit) {
      # Drop if we are below --edgefraction
      $keep = 0;
    } elsif ($outdegree{$src} >= $main::opt_maxdegree ||
             $indegree{$dst} >= $main::opt_maxdegree) {
      # Keep limited number of in/out edges per node
      $keep = 0;
    } else {
      $keep = 1;
    }

    if ($keep) {
      $outdegree{$src}++;
      $indegree{$dst}++;

      # Compute line width based on edge count
      my $fraction = abs($local_total ? (3 * ($n / $local_total)) : 0);
      if ($fraction > 1) { $fraction = 1; }
      my $w = $fraction * 2;
      if ($w < 1 && ($main::opt_web || $main::opt_svg)) {
        # SVG output treats line widths < 1 poorly.
        $w = 1;
      }

      # Dot sometimes segfaults if given edge weights that are too large, so
      # we cap the weights at a large value
      my $edgeweight = abs($n) ** 0.7;
      if ($edgeweight > 100000) { $edgeweight = 100000; }
      $edgeweight = int($edgeweight);

      my $style = sprintf("setlinewidth(%f)", $w);
      if ($x[1] =~ m/\(inline\)/) {
        $style .= ",dashed";
      }

      # Use a slightly squashed function of the edge count as the weight
      printf DOT ("N%s -> N%s [label=%s, weight=%d, style=\"%s\"];\n",
                  $node{$x[0]},
                  $node{$x[1]},
                  Unparse($n),
                  $edgeweight,
                  $style);
    }
  }

  print DOT ("}\n");
  close(DOT);

  if ($main::opt_web || $main::opt_svg) {
    # Rewrite SVG to be more usable inside web browser.
    RewriteSvg(TempName($main::next_tmpfile, "svg"));
  }

  return 1;
}

sub RewriteSvg {
  my $svgfile = shift;

  open(SVG, $svgfile) || die "open temp svg: $!";
  my @svg = <SVG>;
  close(SVG);
  unlink $svgfile;
  my $svg = join('', @svg);

  # Dot's SVG output is
  #
  #    <svg width="___" height="___"
  #     viewBox="___" xmlns=...>
  #    <g id="graph0" transform="...">
  #    ...
  #    </g>
  #    </svg>
  #
  # Change it to
  #
  #    <svg width="100%" height="100%"
  #     xmlns=...>
  #    $svg_javascript
  #    <g id="viewport" transform="translate(0,0)">
  #    <g id="graph0" transform="...">
  #    ...
  #    </g>
  #    </g>
  #    </svg>

  # Fix width, height; drop viewBox.
  $svg =~ s/(?s)<svg width="[^"]+" height="[^"]+"(.*?)viewBox="[^"]+"/<svg width="100%" height="100%"$1/;

  # Insert script, viewport <g> above first <g>
  my $svg_javascript = SvgJavascript();
  my $viewport = "<g id=\"viewport\" transform=\"translate(0,0)\">\n";
  $svg =~ s/<g id="graph\d"/$svg_javascript$viewport$&/;

  # Insert final </g> above </svg>.
  $svg =~ s/(.*)(<\/svg>)/$1<\/g>$2/;
  $svg =~ s/<g id="graph\d"(.*?)/<g id="viewport"$1/;

  if ($main::opt_svg) {
    # --svg: write to standard output.
    print $svg;
  } else {
    # Write back to temporary file.
    open(SVG, ">$svgfile") || die "open $svgfile: $!";
    print SVG $svg;
    close(SVG);
  }
}

sub SvgJavascript {
  return <<'EOF';
<script type="text/ecmascript"><![CDATA[
// SVGPan
// http://www.cyberz.org/blog/2009/12/08/svgpan-a-javascript-svg-panzoomdrag-library/
// Local modification: if(true || ...) below to force panning, never moving.
/**
 *  SVGPan library 1.2
 * ====================
 *
 * Given an unique existing element with id "viewport", including the
 * the library into any SVG adds the following capabilities:
 *
 *  - Mouse panning
 *  - Mouse zooming (using the wheel)
 *  - Object dargging
 *
 * Known issues:
 *
 *  - Zooming (while panning) on Safari has still some issues
 *
 * Releases:
 *
 * 1.2, Sat Mar 20 08:42:50 GMT 2010, Zeng Xiaohui
 *	Fixed a bug with browser mouse handler interaction
 *
 * 1.1, Wed Feb  3 17:39:33 GMT 2010, Zeng Xiaohui
 *	Updated the zoom code to support the mouse wheel on Safari/Chrome
 *
 * 1.0, Andrea Leofreddi
 *	First release
 *
 * This code is licensed under the following BSD license:
 *
 * Copyright 2009-2010 Andrea Leofreddi <a.leofreddi@itcharm.com>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Andrea Leofreddi ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Andrea Leofreddi OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Andrea Leofreddi.
 */
var root = document.documentElement;
var state = 'none', stateTarget, stateOrigin, stateTf;
setupHandlers(root);
/**
 * Register handlers
 */
function setupHandlers(root){
	setAttributes(root, {
		"onmouseup" : "add(evt)",
		"onmousedown" : "handleMouseDown(evt)",
		"onmousemove" : "handleMouseMove(evt)",
		"onmouseup" : "handleMouseUp(evt)",
		//"onmouseout" : "handleMouseUp(evt)", // Decomment this to stop the pan functionality when dragging out of the SVG element
	});
	if(navigator.userAgent.toLowerCase().indexOf('webkit') >= 0)
		window.addEventListener('mousewheel', handleMouseWheel, false); // Chrome/Safari
	else
		window.addEventListener('DOMMouseScroll', handleMouseWheel, false); // Others
	var g = svgDoc.getElementById("svg");
	g.width = "100%";
	g.height = "100%";
}
/**
 * Instance an SVGPoint object with given event coordinates.
 */
function getEventPoint(evt) {
	var p = root.createSVGPoint();
	p.x = evt.clientX;
	p.y = evt.clientY;
	return p;
}
/**
 * Sets the current transform matrix of an element.
 */
function setCTM(element, matrix) {
	var s = "matrix(" + matrix.a + "," + matrix.b + "," + matrix.c + "," + matrix.d + "," + matrix.e + "," + matrix.f + ")";
	element.setAttribute("transform", s);
}
/**
 * Dumps a matrix to a string (useful for debug).
 */
function dumpMatrix(matrix) {
	var s = "[ " + matrix.a + ", " + matrix.c + ", " + matrix.e + "\n  " + matrix.b + ", " + matrix.d + ", " + matrix.f + "\n  0, 0, 1 ]";
	return s;
}
/**
 * Sets attributes of an element.
 */
function setAttributes(element, attributes){
	for (i in attributes)
		element.setAttributeNS(null, i, attributes[i]);
}
/**
 * Handle mouse move event.
 */
function handleMouseWheel(evt) {
	if(evt.preventDefault)
		evt.preventDefault();
	evt.returnValue = false;
	var svgDoc = evt.target.ownerDocument;
	var delta;
	if(evt.wheelDelta)
		delta = evt.wheelDelta / 3600; // Chrome/Safari
	else
		delta = evt.detail / -90; // Mozilla
	var z = 1 + delta; // Zoom factor: 0.9/1.1
	var g = svgDoc.getElementById("viewport");
	var p = getEventPoint(evt);
	p = p.matrixTransform(g.getCTM().inverse());
	// Compute new scale matrix in current mouse position
	var k = root.createSVGMatrix().translate(p.x, p.y).scale(z).translate(-p.x, -p.y);
        setCTM(g, g.getCTM().multiply(k));
	stateTf = stateTf.multiply(k.inverse());
}
/**
 * Handle mouse move event.
 */
function handleMouseMove(evt) {
	if(evt.preventDefault)
		evt.preventDefault();
	evt.returnValue = false;
	var svgDoc = evt.target.ownerDocument;
	var g = svgDoc.getElementById("viewport");
	if(state == 'pan') {
		// Pan mode
		var p = getEventPoint(evt).matrixTransform(stateTf);
		setCTM(g, stateTf.inverse().translate(p.x - stateOrigin.x, p.y - stateOrigin.y));
	} else if(state == 'move') {
		// Move mode
		var p = getEventPoint(evt).matrixTransform(g.getCTM().inverse());
		setCTM(stateTarget, root.createSVGMatrix().translate(p.x - stateOrigin.x, p.y - stateOrigin.y).multiply(g.getCTM().inverse()).multiply(stateTarget.getCTM()));
		stateOrigin = p;
	}
}
/**
 * Handle click event.
 */
function handleMouseDown(evt) {
	if(evt.preventDefault)
		evt.preventDefault();
	evt.returnValue = false;
	var svgDoc = evt.target.ownerDocument;
	var g = svgDoc.getElementById("viewport");
	if(true || evt.target.tagName == "svg") {
		// Pan mode
		state = 'pan';
		stateTf = g.getCTM().inverse();
		stateOrigin = getEventPoint(evt).matrixTransform(stateTf);
	} else {
		// Move mode
		state = 'move';
		stateTarget = evt.target;
		stateTf = g.getCTM().inverse();
		stateOrigin = getEventPoint(evt).matrixTransform(stateTf);
	}
}
/**
 * Handle mouse button release event.
 */
function handleMouseUp(evt) {
	if(evt.preventDefault)
		evt.preventDefault();
	evt.returnValue = false;
	var svgDoc = evt.target.ownerDocument;
	if(state == 'pan' || state == 'move') {
		// Quit pan mode
		state = '';
	}
}
]]></script>
EOF
}

# Provides a map from fullname to shortname for cases where the
# shortname is ambiguous.  The symlist has both the fullname and
# shortname for all symbols, which is usually fine, but sometimes --
# such as overloaded functions -- two different fullnames can map to
# the same shortname.  In that case, we use the address of the
# function to disambiguate the two.  This function fills in a map that
# maps fullnames to modified shortnames in such cases.  If a fullname
# is not present in the map, the 'normal' shortname provided by the
# symlist is the appropriate one to use.
sub FillFullnameToShortnameMap {
  my $symbols = shift;
  my $fullname_to_shortname_map = shift;
  my $shortnames_seen_once = {};
  my $shortnames_seen_more_than_once = {};

  foreach my $symlist (values(%{$symbols})) {
    # TODO(csilvers): deal with inlined symbols too.
    my $shortname = $symlist->[0];
    my $fullname = $symlist->[2];
    if ($fullname !~ /<[0-9a-fA-F]+>$/) {  # fullname doesn't end in an address
      next;       # the only collisions we care about are when addresses differ
    }
    if (defined($shortnames_seen_once->{$shortname}) &&
        $shortnames_seen_once->{$shortname} ne $fullname) {
      $shortnames_seen_more_than_once->{$shortname} = 1;
    } else {
      $shortnames_seen_once->{$shortname} = $fullname;
    }
  }

  foreach my $symlist (values(%{$symbols})) {
    my $shortname = $symlist->[0];
    my $fullname = $symlist->[2];
    # TODO(csilvers): take in a list of addresses we care about, and only
    # store in the map if $symlist->[1] is in that list.  Saves space.
    next if defined($fullname_to_shortname_map->{$fullname});
    if (defined($shortnames_seen_more_than_once->{$shortname})) {
      if ($fullname =~ /<0*([^>]*)>$/) {   # fullname has address at end of it
        $fullname_to_shortname_map->{$fullname} = "$shortname\@$1";
      }
    }
  }
}

# Return a small number that identifies the argument.
# Multiple calls with the same argument will return the same number.
# Calls with different arguments will return different numbers.
sub ShortIdFor {
  my $key = shift;
  my $id = $main::uniqueid{$key};
  if (!defined($id)) {
    $id = keys(%main::uniqueid) + 1;
    $main::uniqueid{$key} = $id;
  }
  return $id;
}

# Translate a stack of addresses into a stack of symbols
sub TranslateStack {
  my $symbols = shift;
  my $fullname_to_shortname_map = shift;
  my $k = shift;

  my @addrs = split(/\n/, $k);
  my @result = ();
  for (my $i = 0; $i <= $#addrs; $i++) {
    my $a = $addrs[$i];

    # Skip large addresses since they sometimes show up as fake entries on RH9
    if (length($a) > 8 && $a gt "7fffffffffffffff") {
      next;
    }

    if ($main::opt_disasm || $main::opt_list) {
      # We want just the address for the key
      push(@result, $a);
      next;
    }

    my $symlist = $symbols->{$a};
    if (!defined($symlist)) {
      $symlist = [$a, "", $a];
    }

    # We can have a sequence of symbols for a particular entry
    # (more than one symbol in the case of inlining).  Callers
    # come before callees in symlist, so walk backwards since
    # the translated stack should contain callees before callers.
    for (my $j = $#{$symlist}; $j >= 2; $j -= 3) {
      my $func = $symlist->[$j-2];
      my $fileline = $symlist->[$j-1];
      my $fullfunc = $symlist->[$j];
      if (defined($fullname_to_shortname_map->{$fullfunc})) {
        $func = $fullname_to_shortname_map->{$fullfunc};
      }
      if ($j > 2) {
        $func = "$func (inline)";
      }

      # Do not merge nodes corresponding to Callback::Run since that
      # causes confusing cycles in dot display.  Instead, we synthesize
      # a unique name for this frame per caller.
      if ($func =~ m/Callback.*::Run$/) {
        my $caller = ($i > 0) ? $addrs[$i-1] : 0;
        $func = "Run#" . ShortIdFor($caller);
      }

      if ($main::opt_addresses) {
        push(@result, "$a $func $fileline");
      } elsif ($main::opt_lines) {
        if ($func eq '??' && $fileline eq '??:0') {
          push(@result, "$a");
        } elsif (!$main::opt_show_addresses) {
          push(@result, "$func $fileline");
        } else {
          push(@result, "$func $fileline ($a)");
        }
      } elsif ($main::opt_functions) {
        if ($func eq '??') {
          push(@result, "$a");
        } elsif (!$main::opt_show_addresses) {
          push(@result, $func);
        } else {
          push(@result, "$func ($a)");
        }
      } elsif ($main::opt_files) {
        if ($fileline eq '??:0' || $fileline eq '') {
          push(@result, "$a");
        } else {
          my $f = $fileline;
          $f =~ s/:\d+$//;
          push(@result, $f);
        }
      } else {
        push(@result, $a);
        last;  # Do not print inlined info
      }
    }
  }

  # print join(",", @addrs), " => ", join(",", @result), "\n";
  return @result;
}

# Generate percent string for a number and a total
sub Percent {
  my $num = shift;
  my $tot = shift;
  if ($tot != 0) {
    return sprintf("%.1f%%", $num * 100.0 / $tot);
  } else {
    return ($num == 0) ? "nan" : (($num > 0) ? "+inf" : "-inf");
  }
}

# Generate pretty-printed form of number
sub Unparse {
  my $num = shift;
  if ($main::profile_type eq 'heap' || $main::profile_type eq 'growth') {
    if ($main::opt_inuse_objects || $main::opt_alloc_objects) {
      return sprintf("%d", $num);
    } else {
      if ($main::opt_show_bytes) {
        return sprintf("%d", $num);
      } else {
        return sprintf("%.1f", $num / 1048576.0);
      }
    }
  } elsif ($main::profile_type eq 'contention' && !$main::opt_contentions) {
    return sprintf("%.3f", $num / 1e9); # Convert nanoseconds to seconds
  } else {
    return sprintf("%d", $num);
  }
}

# Alternate pretty-printed form: 0 maps to "."
sub UnparseAlt {
  my $num = shift;
  if ($num == 0) {
    return ".";
  } else {
    return Unparse($num);
  }
}

# Alternate pretty-printed form: 0 maps to ""
sub HtmlPrintNumber {
  my $num = shift;
  if ($num == 0) {
    return "";
  } else {
    return Unparse($num);
  }
}

# Return output units
sub Units {
  if ($main::profile_type eq 'heap' || $main::profile_type eq 'growth') {
    if ($main::opt_inuse_objects || $main::opt_alloc_objects) {
      return "objects";
    } else {
      if ($main::opt_show_bytes) {
        return "B";
      } else {
        return "MB";
      }
    }
  } elsif ($main::profile_type eq 'contention' && !$main::opt_contentions) {
    return "seconds";
  } else {
    return "samples";
  }
}

##### Profile manipulation code #####

# Generate flattened profile:
# If count is charged to stack [a,b,c,d], in generated profile,
# it will be charged to [a]
sub FlatProfile {
  my $profile = shift;
  my $result = {};
  foreach my $k (keys(%{$profile})) {
    my $count = $profile->{$k};
    my @addrs = split(/\n/, $k);
    if ($#addrs >= 0) {
      AddEntry($result, $addrs[0], $count);
    }
  }
  return $result;
}

# Generate cumulative profile:
# If count is charged to stack [a,b,c,d], in generated profile,
# it will be charged to [a], [b], [c], [d]
sub CumulativeProfile {
  my $profile = shift;
  my $result = {};
  foreach my $k (keys(%{$profile})) {
    my $count = $profile->{$k};
    my @addrs = split(/\n/, $k);
    foreach my $a (@addrs) {
      AddEntry($result, $a, $count);
    }
  }
  return $result;
}

# If the second-youngest PC on the stack is always the same, returns
# that pc.  Otherwise, returns undef.
sub IsSecondPcAlwaysTheSame {
  my $profile = shift;

  my $second_pc = undef;
  foreach my $k (keys(%{$profile})) {
    my @addrs = split(/\n/, $k);
    if ($#addrs < 1) {
      return undef;
    }
    if (not defined $second_pc) {
      $second_pc = $addrs[1];
    } else {
      if ($second_pc ne $addrs[1]) {
        return undef;
      }
    }
  }
  return $second_pc;
}

sub ExtractSymbolLocationInlineStack {
  my $symbols = shift;
  my $address = shift;
  my $stack = shift;
  # 'addr2line' outputs "??:0" for unknown locations; we do the
  # same to be consistent.
  if (exists $symbols->{$address}) {
    my @localinlinestack = @{$symbols->{$address}};
    for (my $i = $#localinlinestack; $i > 0; $i-=3) {
      my $file = $localinlinestack[$i-1];
      my $fn = $localinlinestack[$i-2];
      if ($file eq "?" || $file eq ":0") {
        $file = "??:0";
      }
      my $suffix = "[inline]";
      if ($i == 2) {
        $suffix = "";
      }
      push (@$stack, $file.":".$fn.$suffix);
    }
  }
  else {
      push (@$stack, "??:0:unknown");
  }
}

sub ExtractSymbolNameInlineStack {
  my $symbols = shift;
  my $address = shift;

  my @stack = ();

  if (exists $symbols->{$address}) {
    my @localinlinestack = @{$symbols->{$address}};
    for (my $i = $#localinlinestack; $i > 0; $i-=3) {
      my $file = $localinlinestack[$i-1];
      my $fn = $localinlinestack[$i-0];

      if ($file eq "?" || $file eq ":0") {
        $file = "??:0";
      }
      if ($fn eq '??') {
        # If we can't get the symbol name, at least use the file information.
        $fn = $file;
      }
      my $suffix = "[inline]";
      if ($i == 2) {
        $suffix = "";
      }
      push (@stack, $fn.$suffix);
    }
  }
  else {
    # If we can't get a symbol name, at least fill in the address.
    push (@stack, $address);
  }

  return @stack;
}

sub ExtractSymbolLocation {
  my $symbols = shift;
  my $address = shift;
  # 'addr2line' outputs "??:0" for unknown locations; we do the
  # same to be consistent.
  my $location = "??:0:unknown";
  if (exists $symbols->{$address}) {
    my $file = $symbols->{$address}->[1];
    if ($file eq "?" || $file eq ":0") {
      $file = "??:0"
    }
    $location = $file . ":" . $symbols->{$address}->[0];
  }
  return $location;
}

# Extracts a graph of calls.
sub ExtractCalls {
  my $symbols = shift;
  my $profile = shift;
  my $calls = {};
  while( my ($stack_trace, $count) = each %$profile ) {
    my @address = split(/\n/, $stack_trace);
    my @stack = ();
    ExtractSymbolLocationInlineStack($symbols, $address[0], \@stack);
    for (my $i = 1; $i <= $#address; $i++) {
      ExtractSymbolLocationInlineStack($symbols, $address[$i], \@stack);
    }
    AddEntry($calls, $stack[0], $count);
    for (my $i = 1; $i < $#address; $i++) {
      AddEntry($calls, "$stack[$i] -> $stack[$i-1]", $count);
    }
  }
  return $calls;
}

sub PrintStacksForText {
  my $symbols = shift;
  my $profile = shift;

  while (my ($stack_trace, $count) = each %$profile) {
    my @address = split(/\n/, $stack_trace);
    for (my $i = 0; $i <= $#address; $i++) {
      $address[$i] = sprintf("(%s) %s", $address[$i], ExtractSymbolLocation($symbols, $address[$i]));
    }
    printf("%-8d %s\n\n", $count, join("\n         ", @address));
  }
}

sub PrintCollapsedStacks {
  my $symbols = shift;
  my $profile = shift;

  while (my ($stack_trace, $count) = each %$profile) {
    my @address = split(/\n/, $stack_trace);
    my @names = reverse ( map { ExtractSymbolNameInlineStack($symbols, $_) } @address );
    printf("%s %d\n", join(";", @names), $count);
  }
}

sub RemoveUninterestingFrames {
  my $symbols = shift;
  my $profile = shift;

  # List of function names to skip
  my %skip = ();
  my $skip_regexp = 'NOMATCH';
  if ($main::profile_type eq 'heap' || $main::profile_type eq 'growth') {
    foreach my $name ('calloc',
                      'cfree',
                      'malloc',
                      'free',
                      'memalign',
                      'posix_memalign',
                      'pvalloc',
                      'valloc',
                      'realloc',
                      'tc_calloc',
                      'tc_cfree',
                      'tc_malloc',
                      'tc_free',
                      'tc_memalign',
                      'tc_posix_memalign',
                      'tc_pvalloc',
                      'tc_valloc',
                      'tc_realloc',
                      'tc_new',
                      'tc_delete',
                      'tc_newarray',
                      'tc_deletearray',
                      'tc_new_nothrow',
                      'tc_newarray_nothrow',
                      'do_malloc',
                      '::do_malloc',   # new name -- got moved to an unnamed ns
                      '::do_malloc_or_cpp_alloc',
                      'DoSampledAllocation',
                      'simple_alloc::allocate',
                      '__malloc_alloc_template::allocate',
                      '__builtin_delete',
                      '__builtin_new',
                      '__builtin_vec_delete',
                      '__builtin_vec_new',
                      'operator new',
                      'operator new[]',
                      # The entry to our memory-allocation routines on OS X
                      'malloc_zone_malloc',
                      'malloc_zone_calloc',
                      'malloc_zone_valloc',
                      'malloc_zone_realloc',
                      'malloc_zone_memalign',
                      'malloc_zone_free',
                      # These mark the beginning/end of our custom sections
                      '__start_google_malloc',
                      '__stop_google_malloc',
                      '__start_malloc_hook',
                      '__stop_malloc_hook') {
      $skip{$name} = 1;
      $skip{"_" . $name} = 1;   # Mach (OS X) adds a _ prefix to everything
    }
    # TODO: Remove TCMalloc once everything has been
    # moved into the tcmalloc:: namespace and we have flushed
    # old code out of the system.
    $skip_regexp = "TCMalloc|^tcmalloc::";
  } elsif ($main::profile_type eq 'contention') {
    foreach my $vname ('base::RecordLockProfileData',
                       'base::SubmitMutexProfileData',
                       'base::SubmitSpinLockProfileData',
                       'Mutex::Unlock',
                       'Mutex::UnlockSlow',
                       'Mutex::ReaderUnlock',
                       'MutexLock::~MutexLock',
                       'SpinLock::Unlock',
                       'SpinLock::SlowUnlock',
                       'SpinLockHolder::~SpinLockHolder') {
      $skip{$vname} = 1;
    }
  } elsif ($main::profile_type eq 'cpu' && !$main::opt_no_auto_signal_frames) {
    # Drop signal handlers used for CPU profile collection
    # TODO(dpeng): this should not be necessary; it's taken
    # care of by the general 2nd-pc mechanism below.
    foreach my $name ('ProfileData::Add',           # historical
                      'ProfileData::prof_handler',  # historical
                      'CpuProfiler::prof_handler',
                      '__FRAME_END__',
                      '__pthread_sighandler',
                      '__restore') {
      $skip{$name} = 1;
    }
  } else {
    # Nothing skipped for unknown types
  }

  if ($main::profile_type eq 'cpu') {
    # If all the second-youngest program counters are the same,
    # this STRONGLY suggests that it is an artifact of measurement,
    # i.e., stack frames pushed by the CPU profiler signal handler.
    # Hence, we delete them.
    # (The topmost PC is read from the signal structure, not from
    # the stack, so it does not get involved.)
    while (my $second_pc = IsSecondPcAlwaysTheSame($profile)) {
      my $result = {};
      my $func = '';
      if (exists($symbols->{$second_pc})) {
        $second_pc = $symbols->{$second_pc}->[0];
      }
      if ($main::opt_no_auto_signal_frames) {
        print STDERR "All second stack frames are same: `$second_pc'.\nMight be stack trace capturing bug.\n";
        last;
      }
      print STDERR "Removing $second_pc from all stack traces.\n";
      foreach my $k (keys(%{$profile})) {
        my $count = $profile->{$k};
        my @addrs = split(/\n/, $k);
        my $topaddr = POSIX::strtoul($addrs[0], 16);
        splice @addrs, 1, 1;
        if ($#addrs > 1) {
          my $subtopaddr = POSIX::strtoul($addrs[1], 16);
          if ($subtopaddr + 1 == $topaddr) {
            splice @addrs, 1, 1;
          }
        }
        my $reduced_path = join("\n", @addrs);
        AddEntry($result, $reduced_path, $count);
      }
      $profile = $result;
    }
  }

  my $result = {};
  foreach my $k (keys(%{$profile})) {
    my $count = $profile->{$k};
    my @addrs = split(/\n/, $k);
    my @path = ();
    foreach my $a (@addrs) {
      if (exists($symbols->{$a})) {
        my $func = $symbols->{$a}->[0];
        if ($skip{$func} || ($func =~ m/$skip_regexp/)) {
          next;
        }
      }
      push(@path, $a);
    }
    my $reduced_path = join("\n", @path);
    AddEntry($result, $reduced_path, $count);
  }
  return $result;
}

# Reduce profile to granularity given by user
sub ReduceProfile {
  my $symbols = shift;
  my $profile = shift;
  my $result = {};
  my $fullname_to_shortname_map = {};
  FillFullnameToShortnameMap($symbols, $fullname_to_shortname_map);
  foreach my $k (keys(%{$profile})) {
    my $count = $profile->{$k};
    my @translated = TranslateStack($symbols, $fullname_to_shortname_map, $k);
    my @path = ();
    my %seen = ();
    $seen{''} = 1;      # So that empty keys are skipped
    foreach my $e (@translated) {
      # To avoid double-counting due to recursion, skip a stack-trace
      # entry if it has already been seen
      if (!$seen{$e}) {
        $seen{$e} = 1;
        push(@path, $e);
      }
    }
    my $reduced_path = join("\n", @path);
    AddEntry($result, $reduced_path, $count);
  }
  return $result;
}

# Does the specified symbol array match the regexp?
sub SymbolMatches {
  my $sym = shift;
  my $re = shift;
  if (defined($sym)) {
    for (my $i = 0; $i < $#{$sym}; $i += 3) {
      if ($sym->[$i] =~ m/$re/ || $sym->[$i+1] =~ m/$re/) {
        return 1;
      }
    }
  }
  return 0;
}

# Focus only on paths involving specified regexps
sub FocusProfile {
  my $symbols = shift;
  my $profile = shift;
  my $focus = shift;
  my $result = {};
  foreach my $k (keys(%{$profile})) {
    my $count = $profile->{$k};
    my @addrs = split(/\n/, $k);
    foreach my $a (@addrs) {
      # Reply if it matches either the address/shortname/fileline
      if (($a =~ m/$focus/) || SymbolMatches($symbols->{$a}, $focus)) {
        AddEntry($result, $k, $count);
        last;
      }
    }
  }
  return $result;
}

# Focus only on paths not involving specified regexps
sub IgnoreProfile {
  my $symbols = shift;
  my $profile = shift;
  my $ignore = shift;
  my $result = {};
  foreach my $k (keys(%{$profile})) {
    my $count = $profile->{$k};
    my @addrs = split(/\n/, $k);
    my $matched = 0;
    foreach my $a (@addrs) {
      # Reply if it matches either the address/shortname/fileline
      if (($a =~ m/$ignore/) || SymbolMatches($symbols->{$a}, $ignore)) {
        $matched = 1;
        last;
      }
    }
    if (!$matched) {
      AddEntry($result, $k, $count);
    }
  }
  return $result;
}

# Get total count in profile
sub TotalProfile {
  my $profile = shift;
  my $result = 0;
  foreach my $k (keys(%{$profile})) {
    $result += $profile->{$k};
  }
  return $result;
}

# Add A to B
sub AddProfile {
  my $A = shift;
  my $B = shift;

  my $R = {};
  # add all keys in A
  foreach my $k (keys(%{$A})) {
    my $v = $A->{$k};
    AddEntry($R, $k, $v);
  }
  # add all keys in B
  foreach my $k (keys(%{$B})) {
    my $v = $B->{$k};
    AddEntry($R, $k, $v);
  }
  return $R;
}

# Merges symbol maps
sub MergeSymbols {
  my $A = shift;
  my $B = shift;

  my $R = {};
  foreach my $k (keys(%{$A})) {
    $R->{$k} = $A->{$k};
  }
  if (defined($B)) {
    foreach my $k (keys(%{$B})) {
      $R->{$k} = $B->{$k};
    }
  }
  return $R;
}


# Add A to B
sub AddPcs {
  my $A = shift;
  my $B = shift;

  my $R = {};
  # add all keys in A
  foreach my $k (keys(%{$A})) {
    $R->{$k} = 1
  }
  # add all keys in B
  foreach my $k (keys(%{$B})) {
    $R->{$k} = 1
  }
  return $R;
}

# Subtract B from A
sub SubtractProfile {
  my $A = shift;
  my $B = shift;

  my $R = {};
  foreach my $k (keys(%{$A})) {
    my $v = $A->{$k} - GetEntry($B, $k);
    if ($v < 0 && $main::opt_drop_negative) {
      $v = 0;
    }
    AddEntry($R, $k, $v);
  }
  if (!$main::opt_drop_negative) {
    # Take care of when subtracted profile has more entries
    foreach my $k (keys(%{$B})) {
      if (!exists($A->{$k})) {
        AddEntry($R, $k, 0 - $B->{$k});
      }
    }
  }
  return $R;
}

# Get entry from profile; zero if not present
sub GetEntry {
  my $profile = shift;
  my $k = shift;
  if (exists($profile->{$k})) {
    return $profile->{$k};
  } else {
    return 0;
  }
}

# Add entry to specified profile
sub AddEntry {
  my $profile = shift;
  my $k = shift;
  my $n = shift;
  if (!exists($profile->{$k})) {
    $profile->{$k} = 0;
  }
  $profile->{$k} += $n;
}

# Add a stack of entries to specified profile, and add them to the $pcs
# list.
sub AddEntries {
  my $profile = shift;
  my $pcs = shift;
  my $stack = shift;
  my $count = shift;
  my @k = ();

  foreach my $e (split(/\s+/, $stack)) {
    my $pc = HexExtend($e);
    $pcs->{$pc} = 1;
    push @k, $pc;
  }
  AddEntry($profile, (join "\n", @k), $count);
}

##### Code to profile a server dynamically #####

sub CheckSymbolPage {
  my $url = SymbolPageURL();
  my $command = ShellEscape(@URL_FETCHER, $url);
  open(SYMBOL, "$command |") or error($command);
  my $line = <SYMBOL>;
  $line =~ s/\r//g;         # turn windows-looking lines into unix-looking lines
  close(SYMBOL);
  unless (defined($line)) {
    error("$url doesn't exist\n");
  }

  if ($line =~ /^num_symbols:\s+(\d+)$/) {
    if ($1 == 0) {
      error("Stripped binary. No symbols available.\n");
    }
  } else {
    error("Failed to get the number of symbols from $url\n");
  }
}

sub IsProfileURL {
  my $profile_name = shift;
  if (-f $profile_name) {
    printf STDERR "Using local file $profile_name.\n";
    return 0;
  }
  return 1;
}

sub ParseProfileURL {
  my $profile_name = shift;

  if (!defined($profile_name) || $profile_name eq "") {
    return ();
  }

  # Split profile URL - matches all non-empty strings, so no test.
  $profile_name =~ m,^(https?://)?([^/]+)(.*?)(/|$PROFILES)?$,;

  my $proto = $1 || "http://";
  my $hostport = $2;
  my $prefix = $3;
  my $profile = $4 || "/";

  my $host = $hostport;
  $host =~ s/:.*//;

  my $baseurl = "$proto$hostport$prefix";
  return ($host, $baseurl, $profile);
}

# We fetch symbols from the first profile argument.
sub SymbolPageURL {
  my ($host, $baseURL, $path) = ParseProfileURL($main::pfile_args[0]);
  return "$baseURL$SYMBOL_PAGE";
}

sub FetchProgramName() {
  my ($host, $baseURL, $path) = ParseProfileURL($main::pfile_args[0]);
  my $url = "$baseURL$PROGRAM_NAME_PAGE";
  my $command_line = ShellEscape(@URL_FETCHER, $url);
  open(CMDLINE, "$command_line |") or error($command_line);
  my $cmdline = <CMDLINE>;
  $cmdline =~ s/\r//g;   # turn windows-looking lines into unix-looking lines
  close(CMDLINE);
  error("Failed to get program name from $url\n") unless defined($cmdline);
  $cmdline =~ s/\x00.+//;  # Remove argv[1] and latters.
  $cmdline =~ s!\n!!g;  # Remove LFs.
  return $cmdline;
}

# Gee, curl's -L (--location) option isn't reliable at least
# with its 7.12.3 version.  Curl will forget to post data if
# there is a redirection.  This function is a workaround for
# curl.  Redirection happens on borg hosts.
sub ResolveRedirectionForCurl {
  my $url = shift;
  my $command_line = ShellEscape(@URL_FETCHER, "--head", $url);
  open(CMDLINE, "$command_line |") or error($command_line);
  while (<CMDLINE>) {
    s/\r//g;         # turn windows-looking lines into unix-looking lines
    if (/^Location: (.*)/) {
      $url = $1;
    }
  }
  close(CMDLINE);
  return $url;
}

# Add a timeout flat to URL_FETCHER.  Returns a new list.
sub AddFetchTimeout {
  my $timeout = shift;
  my @fetcher = @_;
  if (defined($timeout)) {
    if (join(" ", @fetcher) =~ m/\bcurl -s/) {
      push(@fetcher, "--max-time", sprintf("%d", $timeout));
    } elsif (join(" ", @fetcher) =~ m/\brpcget\b/) {
      push(@fetcher, sprintf("--deadline=%d", $timeout));
    }
  }
  return @fetcher;
}

# Reads a symbol map from the file handle name given as $1, returning
# the resulting symbol map.  Also processes variables relating to symbols.
# Currently, the only variable processed is 'binary=<value>' which updates
# $main::prog to have the correct program name.
sub ReadSymbols {
  my $in = shift;
  my $map = {};
  while (<$in>) {
    s/\r//g;         # turn windows-looking lines into unix-looking lines
    # Removes all the leading zeroes from the symbols, see comment below.
    if (m/^0x0*([0-9a-f]+)\s+(.+)/) {
      $map->{$1} = $2;
    } elsif (m/^---/) {
      last;
    } elsif (m/^([a-z][^=]*)=(.*)$/ ) {
      my ($variable, $value) = ($1, $2);
      for ($variable, $value) {
        s/^\s+//;
        s/\s+$//;
      }
      if ($variable eq "binary") {
        if ($main::prog ne $UNKNOWN_BINARY && $main::prog ne $value) {
          printf STDERR ("Warning: Mismatched binary name '%s', using '%s'.\n",
                         $main::prog, $value);
        }
        $main::prog = $value;
      } else {
        printf STDERR ("Ignoring unknown variable in symbols list: " .
            "'%s' = '%s'\n", $variable, $value);
      }
    }
  }
  return $map;
}

# Fetches and processes symbols to prepare them for use in the profile output
# code.  If the optional 'symbol_map' arg is not given, fetches symbols from
# $SYMBOL_PAGE for all PC values found in profile.  Otherwise, the raw symbols
# are assumed to have already been fetched into 'symbol_map' and are simply
# extracted and processed.
sub FetchSymbols {
  my $pcset = shift;
  my $symbol_map = shift;

  my %seen = ();
  my @pcs = grep { !$seen{$_}++ } keys(%$pcset);  # uniq

  if (!defined($symbol_map)) {
    my $post_data = join("+", sort((map {"0x" . "$_"} @pcs)));

    open(POSTFILE, ">$main::tmpfile_sym");
    print POSTFILE $post_data;
    close(POSTFILE);

    my $url = SymbolPageURL();

    my $command_line;
    if (join(" ", @URL_FETCHER) =~ m/\bcurl -s/) {
      $url = ResolveRedirectionForCurl($url);
      $command_line = ShellEscape(@URL_FETCHER, "-d", "\@$main::tmpfile_sym",
                                  $url);
    } else {
      $command_line = (ShellEscape(@URL_FETCHER, "--post", $url)
                       . " < " . ShellEscape($main::tmpfile_sym));
    }
    # We use c++filt in case $SYMBOL_PAGE gives us mangled symbols.
    my $escaped_cppfilt = ShellEscape($obj_tool_map{"c++filt"});
    open(SYMBOL, "$command_line | $escaped_cppfilt |") or error($command_line);
    $symbol_map = ReadSymbols(*SYMBOL{IO});
    close(SYMBOL);
  }

  my $symbols = {};
  foreach my $pc (@pcs) {
    my $fullname;
    # For 64 bits binaries, symbols are extracted with 8 leading zeroes.
    # Then /symbol reads the long symbols in as uint64, and outputs
    # the result with a "0x%08llx" format which get rid of the zeroes.
    # By removing all the leading zeroes in both $pc and the symbols from
    # /symbol, the symbols match and are retrievable from the map.
    my $shortpc = $pc;
    $shortpc =~ s/^0*//;
    # Each line may have a list of names, which includes the function
    # and also other functions it has inlined.  They are separated (in
    # PrintSymbolizedProfile), by --, which is illegal in function names.
    my $fullnames;
    if (defined($symbol_map->{$shortpc})) {
      $fullnames = $symbol_map->{$shortpc};
    } else {
      $fullnames = "0x" . $pc;  # Just use addresses
    }
    my $sym = [];
    $symbols->{$pc} = $sym;
    foreach my $fullname (split("--", $fullnames)) {
      my $name = ShortFunctionName($fullname);
      push(@{$sym}, $name, "?", $fullname);
    }
  }
  return $symbols;
}

sub BaseName {
  my $file_name = shift;
  $file_name =~ s!^.*/!!;  # Remove directory name
  return $file_name;
}

sub MakeProfileBaseName {
  my ($binary_name, $profile_name) = @_;
  my ($host, $baseURL, $path) = ParseProfileURL($profile_name);
  my $binary_shortname = BaseName($binary_name);
  return sprintf("%s.%s.%s",
                 $binary_shortname, $main::op_time, $host);
}

sub FetchDynamicProfile {
  my $binary_name = shift;
  my $profile_name = shift;
  my $fetch_name_only = shift;
  my $encourage_patience = shift;

  if (!IsProfileURL($profile_name)) {
    return $profile_name;
  } else {
    my ($host, $baseURL, $path) = ParseProfileURL($profile_name);
    if ($path eq "" || $path eq "/") {
      # Missing type specifier defaults to cpu-profile
      $path = $PROFILE_PAGE;
    }

    my $profile_file = MakeProfileBaseName($binary_name, $profile_name);

    my $url = "$baseURL$path";
    my $fetch_timeout = undef;
    if ($path =~ m/$PROFILE_PAGE|$PMUPROFILE_PAGE/) {
      if ($path =~ m/[?]/) {
        $url .= "&";
      } else {
        $url .= "?";
      }
      $url .= sprintf("seconds=%d", $main::opt_seconds);
      $fetch_timeout = $main::opt_seconds * 1.01 + 60;
    } else {
      # For non-CPU profiles, we add a type-extension to
      # the target profile file name.
      my $suffix = $path;
      $suffix =~ s,/,.,g;
      $profile_file .= $suffix;
    }

    my $profile_dir = $ENV{"PPROF_TMPDIR"} || ($ENV{HOME} . "/pprof");
    if (! -d $profile_dir) {
      mkdir($profile_dir)
          || die("Unable to create profile directory $profile_dir: $!\n");
    }
    my $tmp_profile = "$profile_dir/.tmp.$profile_file";
    my $real_profile = "$profile_dir/$profile_file";

    if ($fetch_name_only > 0) {
      return $real_profile;
    }

    my @fetcher = AddFetchTimeout($fetch_timeout, @URL_FETCHER);
    my $cmd = ShellEscape(@fetcher, $url) . " > " . ShellEscape($tmp_profile);
    if ($path =~ m/$PROFILE_PAGE|$PMUPROFILE_PAGE|$CENSUSPROFILE_PAGE/){
      print STDERR "Gathering CPU profile from $url for $main::opt_seconds seconds to\n  ${real_profile}\n";
      if ($encourage_patience) {
        print STDERR "Be patient...\n";
      }
    } else {
      print STDERR "Fetching $path profile from $url to\n  ${real_profile}\n";
    }

    (system($cmd) == 0) || error("Failed to get profile: $cmd: $!\n");
    (system("mv", $tmp_profile, $real_profile) == 0) || error("Unable to rename profile\n");
    print STDERR "Wrote profile to $real_profile\n";
    $main::collected_profile = $real_profile;
    return $main::collected_profile;
  }
}

# Collect profiles in parallel
sub FetchDynamicProfiles {
  my $items = scalar(@main::pfile_args);
  my $levels = log($items) / log(2);

  if ($items == 1) {
    $main::profile_files[0] = FetchDynamicProfile($main::prog, $main::pfile_args[0], 0, 1);
  } else {
    # math rounding issues
    if ((2 ** $levels) < $items) {
     $levels++;
    }
    my $count = scalar(@main::pfile_args);
    for (my $i = 0; $i < $count; $i++) {
      $main::profile_files[$i] = FetchDynamicProfile($main::prog, $main::pfile_args[$i], 1, 0);
    }
    print STDERR "Fetching $count profiles, Be patient...\n";
    FetchDynamicProfilesRecurse($levels, 0, 0);
    $main::collected_profile = join(" \\\n    ", @main::profile_files);
  }
}

# Recursively fork a process to get enough processes
# collecting profiles
sub FetchDynamicProfilesRecurse {
  my $maxlevel = shift;
  my $level = shift;
  my $position = shift;

  if (my $pid = fork()) {
    $position = 0 | ($position << 1);
    TryCollectProfile($maxlevel, $level, $position);
    wait;
  } else {
    $position = 1 | ($position << 1);
    TryCollectProfile($maxlevel, $level, $position);
    cleanup();
    exit(0);
  }
}

# Collect a single profile
sub TryCollectProfile {
  my $maxlevel = shift;
  my $level = shift;
  my $position = shift;

  if ($level >= ($maxlevel - 1)) {
    if ($position < scalar(@main::pfile_args)) {
      FetchDynamicProfile($main::prog, $main::pfile_args[$position], 0, 0);
    }
  } else {
    FetchDynamicProfilesRecurse($maxlevel, $level+1, $position);
  }
}

##### Parsing code #####

# Provide a small streaming-read module to handle very large
# cpu-profile files.  Stream in chunks along a sliding window.
# Provides an interface to get one 'slot', correctly handling
# endian-ness differences.  A slot is one 32-bit or 64-bit word
# (depending on the input profile).  We tell endianness and bit-size
# for the profile by looking at the first 8 bytes: in cpu profiles,
# the second slot is always 3 (we'll accept anything that's not 0).
BEGIN {
  package CpuProfileStream;

  sub new {
    my ($class, $file, $fname) = @_;
    my $self = { file        => $file,
                 base        => 0,
                 stride      => 512 * 1024,   # must be a multiple of bitsize/8
                 slots       => [],
                 unpack_code => "",           # N for big-endian, V for little
                 perl_is_64bit => 1,          # matters if profile is 64-bit
    };
    bless $self, $class;
    # Let unittests adjust the stride
    if ($main::opt_test_stride > 0) {
      $self->{stride} = $main::opt_test_stride;
    }
    # Read the first two slots to figure out bitsize and endianness.
    my $slots = $self->{slots};
    my $str;
    read($self->{file}, $str, 8);
    # Set the global $address_length based on what we see here.
    # 8 is 32-bit (8 hexadecimal chars); 16 is 64-bit (16 hexadecimal chars).
    $address_length = ($str eq (chr(0)x8)) ? 16 : 8;
    if ($address_length == 8) {
      if (substr($str, 6, 2) eq chr(0)x2) {
        $self->{unpack_code} = 'V';  # Little-endian.
      } elsif (substr($str, 4, 2) eq chr(0)x2) {
        $self->{unpack_code} = 'N';  # Big-endian
      } else {
        ::error("$fname: header size >= 2**16\n");
      }
      @$slots = unpack($self->{unpack_code} . "*", $str);
    } else {
      # If we're a 64-bit profile, check if we're a 64-bit-capable
      # perl.  Otherwise, each slot will be represented as a float
      # instead of an int64, losing precision and making all the
      # 64-bit addresses wrong.  We won't complain yet, but will
      # later if we ever see a value that doesn't fit in 32 bits.
      my $has_q = 0;
      eval { $has_q = pack("Q", "1") ? 1 : 1; };
      if (!$has_q) {
        $self->{perl_is_64bit} = 0;
      }
      read($self->{file}, $str, 8);
      if (substr($str, 4, 4) eq chr(0)x4) {
        # We'd love to use 'Q', but it's a) not universal, b) not endian-proof.
        $self->{unpack_code} = 'V';  # Little-endian.
      } elsif (substr($str, 0, 4) eq chr(0)x4) {
        $self->{unpack_code} = 'N';  # Big-endian
      } else {
        ::error("$fname: header size >= 2**32\n");
      }
      my @pair = unpack($self->{unpack_code} . "*", $str);
      # Since we know one of the pair is 0, it's fine to just add them.
      @$slots = (0, $pair[0] + $pair[1]);
    }
    return $self;
  }

  # Load more data when we access slots->get(X) which is not yet in memory.
  sub overflow {
    my ($self) = @_;
    my $slots = $self->{slots};
    $self->{base} += $#$slots + 1;   # skip over data we're replacing
    my $str;
    read($self->{file}, $str, $self->{stride});
    if ($address_length == 8) {      # the 32-bit case
      # This is the easy case: unpack provides 32-bit unpacking primitives.
      @$slots = unpack($self->{unpack_code} . "*", $str);
    } else {
      # We need to unpack 32 bits at a time and combine.
      my @b32_values = unpack($self->{unpack_code} . "*", $str);
      my @b64_values = ();
      for (my $i = 0; $i < $#b32_values; $i += 2) {
        # TODO(csilvers): if this is a 32-bit perl, the math below
        #    could end up in a too-large int, which perl will promote
        #    to a double, losing necessary precision.  Deal with that.
        #    Right now, we just die.
        my ($lo, $hi) = ($b32_values[$i], $b32_values[$i+1]);
        if ($self->{unpack_code} eq 'N') {    # big-endian
          ($lo, $hi) = ($hi, $lo);
        }
        my $value = $lo + $hi * (2**32);
        if (!$self->{perl_is_64bit} &&   # check value is exactly represented
            (($value % (2**32)) != $lo || int($value / (2**32)) != $hi)) {
          ::error("Need a 64-bit perl to process this 64-bit profile.\n");
        }
        push(@b64_values, $value);
      }
      @$slots = @b64_values;
    }
  }

  # Access the i-th long in the file (logically), or -1 at EOF.
  sub get {
    my ($self, $idx) = @_;
    my $slots = $self->{slots};
    while ($#$slots >= 0) {
      if ($idx < $self->{base}) {
        # The only time we expect a reference to $slots[$i - something]
        # after referencing $slots[$i] is reading the very first header.
        # Since $stride > |header|, that shouldn't cause any lookback
        # errors.  And everything after the header is sequential.
        print STDERR "Unexpected look-back reading CPU profile";
        return -1;   # shrug, don't know what better to return
      } elsif ($idx > $self->{base} + $#$slots) {
        $self->overflow();
      } else {
        return $slots->[$idx - $self->{base}];
      }
    }
    # If we get here, $slots is [], which means we've reached EOF
    return -1;  # unique since slots is supposed to hold unsigned numbers
  }
}

# Reads the top, 'header' section of a profile, and returns the last
# line of the header, commonly called a 'header line'.  The header
# section of a profile consists of zero or more 'command' lines that
# are instructions to pprof, which pprof executes when reading the
# header.  All 'command' lines start with a %.  After the command
# lines is the 'header line', which is a profile-specific line that
# indicates what type of profile it is, and perhaps other global
# information about the profile.  For instance, here's a header line
# for a heap profile:
#   heap profile:     53:    38236 [  5525:  1284029] @ heapprofile
# For historical reasons, the CPU profile does not contain a text-
# readable header line.  If the profile looks like a CPU profile,
# this function returns "".  If no header line could be found, this
# function returns undef.
#
# The following commands are recognized:
#   %warn -- emit the rest of this line to stderr, prefixed by 'WARNING:'
#
# The input file should be in binmode.
sub ReadProfileHeader {
  local *PROFILE = shift;
  my $firstchar = "";
  my $line = "";
  read(PROFILE, $firstchar, 1);
  seek(PROFILE, -1, 1);                    # unread the firstchar
  if ($firstchar !~ /[[:print:]]/) {       # is not a text character
    return "";
  }
  while (defined($line = <PROFILE>)) {
    $line =~ s/\r//g;   # turn windows-looking lines into unix-looking lines
    if ($line =~ /^%warn\s+(.*)/) {        # 'warn' command
      # Note this matches both '%warn blah\n' and '%warn\n'.
      print STDERR "WARNING: $1\n";        # print the rest of the line
    } elsif ($line =~ /^%/) {
      print STDERR "Ignoring unknown command from profile header: $line";
    } else {
      # End of commands, must be the header line.
      return $line;
    }
  }
  return undef;     # got to EOF without seeing a header line
}

sub IsSymbolizedProfileFile {
  my $file_name = shift;
  if (!(-e $file_name) || !(-r $file_name)) {
    return 0;
  }
  # Check if the file contains a symbol-section marker.
  open(TFILE, "<$file_name");
  binmode TFILE;
  my $firstline = ReadProfileHeader(*TFILE);
  close(TFILE);
  if (!$firstline) {
    return 0;
  }
  $SYMBOL_PAGE =~ m,[^/]+$,;    # matches everything after the last slash
  my $symbol_marker = $&;
  return $firstline =~ /^--- *$symbol_marker/;
}

# Parse profile generated by common/profiler.cc and return a reference
# to a map:
#      $result->{version}     Version number of profile file
#      $result->{period}      Sampling period (in microseconds)
#      $result->{profile}     Profile object
#      $result->{map}         Memory map info from profile
#      $result->{pcs}         Hash of all PC values seen, key is hex address
sub ReadProfile {
  my $prog = shift;
  my $fname = shift;
  my $result;            # return value

  $CONTENTION_PAGE =~ m,[^/]+$,;    # matches everything after the last slash
  my $contention_marker = $&;
  $GROWTH_PAGE  =~ m,[^/]+$,;    # matches everything after the last slash
  my $growth_marker = $&;
  $SYMBOL_PAGE =~ m,[^/]+$,;    # matches everything after the last slash
  my $symbol_marker = $&;
  $PROFILE_PAGE =~ m,[^/]+$,;    # matches everything after the last slash
  my $profile_marker = $&;

  # Look at first line to see if it is a heap or a CPU profile.
  # CPU profile may start with no header at all, and just binary data
  # (starting with \0\0\0\0) -- in that case, don't try to read the
  # whole firstline, since it may be gigabytes(!) of data.
  open(PROFILE, "<$fname") || error("$fname: $!\n");
  binmode PROFILE;      # New perls do UTF-8 processing
  my $header = ReadProfileHeader(*PROFILE);
  if (!defined($header)) {   # means "at EOF"
    error("Profile is empty.\n");
  }

  my $symbols;
  if ($header =~ m/^--- *$symbol_marker/o) {
    # Verify that the user asked for a symbolized profile
    if (!$main::use_symbolized_profile) {
      # we have both a binary and symbolized profiles, abort
      error("FATAL ERROR: Symbolized profile\n   $fname\ncannot be used with " .
            "a binary arg. Try again without passing\n   $prog\n");
    }
    # Read the symbol section of the symbolized profile file.
    $symbols = ReadSymbols(*PROFILE{IO});
    # Read the next line to get the header for the remaining profile.
    $header = ReadProfileHeader(*PROFILE) || "";
  }

  $main::profile_type = '';
  if ($header =~ m/^heap profile:.*$growth_marker/o) {
    $main::profile_type = 'growth';
    $result =  ReadHeapProfile($prog, *PROFILE, $header);
  } elsif ($header =~ m/^heap profile:/) {
    $main::profile_type = 'heap';
    $result =  ReadHeapProfile($prog, *PROFILE, $header);
  } elsif ($header =~ m/^--- *$contention_marker/o) {
    $main::profile_type = 'contention';
    $result = ReadSynchProfile($prog, *PROFILE);
  } elsif ($header =~ m/^--- *Stacks:/) {
    print STDERR
      "Old format contention profile: mistakenly reports " .
      "condition variable signals as lock contentions.\n";
    $main::profile_type = 'contention';
    $result = ReadSynchProfile($prog, *PROFILE);
  } elsif ($header =~ m/^--- *$profile_marker/) {
    # the binary cpu profile data starts immediately after this line
    $main::profile_type = 'cpu';
    $result = ReadCPUProfile($prog, $fname, *PROFILE);
  } else {
    if (defined($symbols)) {
      # a symbolized profile contains a format we don't recognize, bail out
      error("$fname: Cannot recognize profile section after symbols.\n");
    }
    # no ascii header present -- must be a CPU profile
    $main::profile_type = 'cpu';
    $result = ReadCPUProfile($prog, $fname, *PROFILE);
  }

  close(PROFILE);

  # if we got symbols along with the profile, return those as well
  if (defined($symbols)) {
    $result->{symbols} = $symbols;
  }

  return $result;
}

# Subtract one from caller pc so we map back to call instr.
# However, don't do this if we're reading a symbolized profile
# file, in which case the subtract-one was done when the file
# was written.
#
# We apply the same logic to all readers, though ReadCPUProfile uses an
# independent implementation.
sub FixCallerAddresses {
  my $stack = shift;
  if ($main::use_symbolized_profile) {
    return $stack;
  } else {
    $stack =~ /(\s)/;
    my $delimiter = $1;
    my @addrs = split(' ', $stack);
    my @fixedaddrs;
    $#fixedaddrs = $#addrs;
    if ($#addrs >= 0) {
      $fixedaddrs[0] = $addrs[0];
    }
    for (my $i = 1; $i <= $#addrs; $i++) {
      $fixedaddrs[$i] = AddressSub($addrs[$i], "0x1");
    }
    return join $delimiter, @fixedaddrs;
  }
}

# CPU profile reader
sub ReadCPUProfile {
  my $prog = shift;
  my $fname = shift;       # just used for logging
  local *PROFILE = shift;
  my $version;
  my $period;
  my $i;
  my $profile = {};
  my $pcs = {};

  # Parse string into array of slots.
  my $slots = CpuProfileStream->new(*PROFILE, $fname);

  # Read header.  The current header version is a 5-element structure
  # containing:
  #   0: header count (always 0)
  #   1: header "words" (after this one: 3)
  #   2: format version (0)
  #   3: sampling period (usec)
  #   4: unused padding (always 0)
  if ($slots->get(0) != 0 ) {
    error("$fname: not a profile file, or old format profile file\n");
  }
  $i = 2 + $slots->get(1);
  $version = $slots->get(2);
  $period = $slots->get(3);
  # Do some sanity checking on these header values.
  if ($version > (2**32) || $period > (2**32) || $i > (2**32) || $i < 5) {
    error("$fname: not a profile file, or corrupted profile file\n");
  }

  # Parse profile
  while ($slots->get($i) != -1) {
    my $n = $slots->get($i++);
    my $d = $slots->get($i++);
    if ($d > (2**16)) {  # TODO(csilvers): what's a reasonable max-stack-depth?
      my $addr = sprintf("0%o", $i * ($address_length == 8 ? 4 : 8));
      print STDERR "At index $i (address $addr):\n";
      error("$fname: stack trace depth >= 2**32\n");
    }
    if ($slots->get($i) == 0) {
      # End of profile data marker
      $i += $d;
      last;
    }

    # Make key out of the stack entries
    my @k = ();
    for (my $j = 0; $j < $d; $j++) {
      my $pc = $slots->get($i+$j);
      # Subtract one from caller pc so we map back to call instr.
      # However, don't do this if we're reading a symbolized profile
      # file, in which case the subtract-one was done when the file
      # was written.
      if ($j > 0 && !$main::use_symbolized_profile) {
        $pc--;
      }
      $pc = sprintf("%0*x", $address_length, $pc);
      $pcs->{$pc} = 1;
      push @k, $pc;
    }

    AddEntry($profile, (join "\n", @k), $n);
    $i += $d;
  }

  # Parse map
  my $map = '';
  seek(PROFILE, $i * ($address_length / 2), 0);
  read(PROFILE, $map, (stat PROFILE)[7]);

  my $r = {};
  $r->{version} = $version;
  $r->{period} = $period;
  $r->{profile} = $profile;
  $r->{libs} = ParseLibraries($prog, $map, $pcs);
  $r->{pcs} = $pcs;

  return $r;
}

sub ReadHeapProfile {
  my $prog = shift;
  local *PROFILE = shift;
  my $header = shift;

  my $index = 1;
  if ($main::opt_inuse_space) {
    $index = 1;
  } elsif ($main::opt_inuse_objects) {
    $index = 0;
  } elsif ($main::opt_alloc_space) {
    $index = 3;
  } elsif ($main::opt_alloc_objects) {
    $index = 2;
  }

  # Find the type of this profile.  The header line looks like:
  #    heap profile:   1246:  8800744 [  1246:  8800744] @ <heap-url>/266053
  # There are two pairs <count: size>, the first inuse objects/space, and the
  # second allocated objects/space.  This is followed optionally by a profile
  # type, and if that is present, optionally by a sampling frequency.
  # For remote heap profiles (v1):
  # The interpretation of the sampling frequency is that the profiler, for
  # each sample, calculates a uniformly distributed random integer less than
  # the given value, and records the next sample after that many bytes have
  # been allocated.  Therefore, the expected sample interval is half of the
  # given frequency.  By default, if not specified, the expected sample
  # interval is 128KB.  Only remote-heap-page profiles are adjusted for
  # sample size.
  # For remote heap profiles (v2):
  # The sampling frequency is the rate of a Poisson process. This means that
  # the probability of sampling an allocation of size X with sampling rate Y
  # is 1 - exp(-X/Y)
  # For version 2, a typical header line might look like this:
  # heap profile:   1922: 127792360 [  1922: 127792360] @ <heap-url>_v2/524288
  # the trailing number (524288) is the sampling rate. (Version 1 showed
  # double the 'rate' here)
  my $sampling_algorithm = 0;
  my $sample_adjustment = 0;
  chomp($header);
  my $type = "unknown";
  if ($header =~ m"^heap profile:\s*(\d+):\s+(\d+)\s+\[\s*(\d+):\s+(\d+)\](\s*@\s*([^/]*)(/(\d+))?)?") {
    if (defined($6) && ($6 ne '')) {
      $type = $6;
      my $sample_period = $8;
      # $type is "heapprofile" for profiles generated by the
      # heap-profiler, and either "heap" or "heap_v2" for profiles
      # generated by sampling directly within tcmalloc.  It can also
      # be "growth" for heap-growth profiles.  The first is typically
      # found for profiles generated locally, and the others for
      # remote profiles.
      if (($type eq "heapprofile") || ($type !~ /heap/) ) {
        # No need to adjust for the sampling rate with heap-profiler-derived data
        $sampling_algorithm = 0;
      } elsif ($type =~ /_v2/) {
        $sampling_algorithm = 2;     # version 2 sampling
        if (defined($sample_period) && ($sample_period ne '')) {
          $sample_adjustment = int($sample_period);
        }
      } else {
        $sampling_algorithm = 1;     # version 1 sampling
        if (defined($sample_period) && ($sample_period ne '')) {
          $sample_adjustment = int($sample_period)/2;
        }
      }
    } else {
      # We detect whether or not this is a remote-heap profile by checking
      # that the total-allocated stats ($n2,$s2) are exactly the
      # same as the in-use stats ($n1,$s1).  It is remotely conceivable
      # that a non-remote-heap profile may pass this check, but it is hard
      # to imagine how that could happen.
      # In this case it's so old it's guaranteed to be remote-heap version 1.
      my ($n1, $s1, $n2, $s2) = ($1, $2, $3, $4);
      if (($n1 == $n2) && ($s1 == $s2)) {
        # This is likely to be a remote-heap based sample profile
        $sampling_algorithm = 1;
      }
    }
  }

  if ($sampling_algorithm > 0) {
    # For remote-heap generated profiles, adjust the counts and sizes to
    # account for the sample rate (we sample once every 128KB by default).
    if ($sample_adjustment == 0) {
      # Turn on profile adjustment.
      $sample_adjustment = 128*1024;
      print STDERR "Adjusting heap profiles for 1-in-128KB sampling rate\n";
    } else {
      printf STDERR ("Adjusting heap profiles for 1-in-%d sampling rate\n",
                     $sample_adjustment);
    }
    if ($sampling_algorithm > 1) {
      # We don't bother printing anything for the original version (version 1)
      printf STDERR "Heap version $sampling_algorithm\n";
    }
  }

  my $profile = {};
  my $pcs = {};
  my $map = "";

  while (<PROFILE>) {
    s/\r//g;         # turn windows-looking lines into unix-looking lines
    if (/^MAPPED_LIBRARIES:/) {
      # Read the /proc/self/maps data
      while (<PROFILE>) {
        s/\r//g;         # turn windows-looking lines into unix-looking lines
        $map .= $_;
      }
      last;
    }

    if (/^--- Memory map:/) {
      # Read /proc/self/maps data as formatted by DumpAddressMap()
      my $buildvar = "";
      while (<PROFILE>) {
        s/\r//g;         # turn windows-looking lines into unix-looking lines
        # Parse "build=<dir>" specification if supplied
        if (m/^\s*build=(.*)\n/) {
          $buildvar = $1;
        }

        # Expand "$build" variable if available
        $_ =~ s/\$build\b/$buildvar/g;

        $map .= $_;
      }
      last;
    }

    # Read entry of the form:
    #  <count1>: <bytes1> [<count2>: <bytes2>] @ a1 a2 a3 ... an
    s/^\s*//;
    s/\s*$//;
    if (m/^\s*(\d+):\s+(\d+)\s+\[\s*(\d+):\s+(\d+)\]\s+@\s+(.*)$/) {
      my $stack = $5;
      my ($n1, $s1, $n2, $s2) = ($1, $2, $3, $4);

      if ($sample_adjustment) {
        if ($sampling_algorithm == 2) {
          # Remote-heap version 2
          # The sampling frequency is the rate of a Poisson process.
          # This means that the probability of sampling an allocation of
          # size X with sampling rate Y is 1 - exp(-X/Y)
          if ($n1 != 0) {
            my $ratio = (($s1*1.0)/$n1)/($sample_adjustment);
            my $scale_factor = 1/(1 - exp(-$ratio));
            $n1 *= $scale_factor;
            $s1 *= $scale_factor;
          }
          if ($n2 != 0) {
            my $ratio = (($s2*1.0)/$n2)/($sample_adjustment);
            my $scale_factor = 1/(1 - exp(-$ratio));
            $n2 *= $scale_factor;
            $s2 *= $scale_factor;
          }
        } else {
          # Remote-heap version 1
          my $ratio;
          $ratio = (($s1*1.0)/$n1)/($sample_adjustment);
          if ($ratio < 1) {
            $n1 /= $ratio;
            $s1 /= $ratio;
          }
          $ratio = (($s2*1.0)/$n2)/($sample_adjustment);
          if ($ratio < 1) {
            $n2 /= $ratio;
            $s2 /= $ratio;
          }
        }
      }

      my @counts = ($n1, $s1, $n2, $s2);
      $stack = FixCallerAddresses($stack);
      push @stackTraces, "$n1 $s1 $n2 $s2 $stack";
      AddEntries($profile, $pcs, $stack, $counts[$index]);
    }
  }

  my $r = {};
  $r->{version} = "heap";
  $r->{period} = 1;
  $r->{profile} = $profile;
  $r->{libs} = ParseLibraries($prog, $map, $pcs);
  $r->{pcs} = $pcs;
  return $r;
}

sub ReadSynchProfile {
  my $prog = shift;
  local *PROFILE = shift;
  my $header = shift;

  my $map = '';
  my $profile = {};
  my $pcs = {};
  my $sampling_period = 1;
  my $cyclespernanosec = 2.8;   # Default assumption for old binaries
  my $seen_clockrate = 0;
  my $line;

  my $index = 0;
  if ($main::opt_total_delay) {
    $index = 0;
  } elsif ($main::opt_contentions) {
    $index = 1;
  } elsif ($main::opt_mean_delay) {
    $index = 2;
  }

  while ( $line = <PROFILE> ) {
    $line =~ s/\r//g;      # turn windows-looking lines into unix-looking lines
    if ( $line =~ /^\s*(\d+)\s+(\d+) \@\s*(.*?)\s*$/ ) {
      my ($cycles, $count, $stack) = ($1, $2, $3);

      # Convert cycles to nanoseconds
      $cycles /= $cyclespernanosec;

      # Adjust for sampling done by application
      $cycles *= $sampling_period;
      $count *= $sampling_period;

      my @values = ($cycles, $count, $cycles / $count);
      AddEntries($profile, $pcs, FixCallerAddresses($stack), $values[$index]);

    } elsif ( $line =~ /^(slow release).*thread \d+  \@\s*(.*?)\s*$/ ||
              $line =~ /^\s*(\d+) \@\s*(.*?)\s*$/ ) {
      my ($cycles, $stack) = ($1, $2);
      if ($cycles !~ /^\d+$/) {
        next;
      }

      # Convert cycles to nanoseconds
      $cycles /= $cyclespernanosec;

      # Adjust for sampling done by application
      $cycles *= $sampling_period;

      AddEntries($profile, $pcs, FixCallerAddresses($stack), $cycles);

    } elsif ( $line =~ m/^([a-z][^=]*)=(.*)$/ ) {
      my ($variable, $value) = ($1,$2);
      for ($variable, $value) {
        s/^\s+//;
        s/\s+$//;
      }
      if ($variable eq "cycles/second") {
        $cyclespernanosec = $value / 1e9;
        $seen_clockrate = 1;
      } elsif ($variable eq "sampling period") {
        $sampling_period = $value;
      } elsif ($variable eq "ms since reset") {
        # Currently nothing is done with this value in pprof
        # So we just silently ignore it for now
      } elsif ($variable eq "discarded samples") {
        # Currently nothing is done with this value in pprof
        # So we just silently ignore it for now
      } else {
        printf STDERR ("Ignoring unnknown variable in /contention output: " .
                       "'%s' = '%s'\n",$variable,$value);
      }
    } else {
      # Memory map entry
      $map .= $line;
    }
  }

  if (!$seen_clockrate) {
    printf STDERR ("No cycles/second entry in profile; Guessing %.1f GHz\n",
                   $cyclespernanosec);
  }

  my $r = {};
  $r->{version} = 0;
  $r->{period} = $sampling_period;
  $r->{profile} = $profile;
  $r->{libs} = ParseLibraries($prog, $map, $pcs);
  $r->{pcs} = $pcs;
  return $r;
}

# Given a hex value in the form "0x1abcd" or "1abcd", return either
# "0001abcd" or "000000000001abcd", depending on the current (global)
# address length.
sub HexExtend {
  my $addr = shift;

  $addr =~ s/^(0x)?0*//;
  my $zeros_needed = $address_length - length($addr);
  if ($zeros_needed < 0) {
    printf STDERR "Warning: address $addr is longer than address length $address_length\n";
    return $addr;
  }
  return ("0" x $zeros_needed) . $addr;
}

##### Symbol extraction #####

# Aggressively search the lib_prefix values for the given library
# If all else fails, just return the name of the library unmodified.
# If the lib_prefix is "/my/path,/other/path" and $file is "/lib/dir/mylib.so"
# it will search the following locations in this order, until it finds a file:
#   /my/path/lib/dir/mylib.so
#   /other/path/lib/dir/mylib.so
#   /my/path/dir/mylib.so
#   /other/path/dir/mylib.so
#   /my/path/mylib.so
#   /other/path/mylib.so
#   /lib/dir/mylib.so              (returned as last resort)
sub FindLibrary {
  my $file = shift;
  my $suffix = $file;

  # Search for the library as described above
  do {
    foreach my $prefix (@prefix_list) {
      my $fullpath = $prefix . $suffix;
      if (-e $fullpath) {
        return $fullpath;
      }
    }
  } while ($suffix =~ s|^/[^/]+/|/|);
  return $file;
}

# Return path to library with debugging symbols.
# For libc libraries, the copy in /usr/lib/debug contains debugging symbols
sub DebuggingLibrary {
  my $file = shift;
  if ($file =~ m|^/| && -f "/usr/lib/debug$file") {
    return "/usr/lib/debug$file";
  }
  if ($file =~ m|^/| && -f "/usr/lib/debug$file.debug") {
    return "/usr/lib/debug$file.debug";
  }
  return undef;
}

# Parse text section header of a library using objdump
sub ParseTextSectionHeaderFromObjdump {
  my $lib = shift;

  my $size = undef;
  my $vma;
  my $file_offset;
  # Get objdump output from the library file to figure out how to
  # map between mapped addresses and addresses in the library.
  my $cmd = ShellEscape($obj_tool_map{"objdump"}, "-h", $lib);
  open(OBJDUMP, "$cmd |") || error("$cmd: $!\n");
  while (<OBJDUMP>) {
    s/\r//g;         # turn windows-looking lines into unix-looking lines
    # Idx Name          Size      VMA       LMA       File off  Algn
    #  10 .text         00104b2c  420156f0  420156f0  000156f0  2**4
    # For 64-bit objects, VMA and LMA will be 16 hex digits, size and file
    # offset may still be 8.  But AddressSub below will still handle that.
    my @x = split;
    if (($#x >= 6) && ($x[1] eq '.text')) {
      $size = $x[2];
      $vma = $x[3];
      $file_offset = $x[5];
      last;
    }
  }
  close(OBJDUMP);

  if (!defined($size)) {
    return undef;
  }

  my $r = {};
  $r->{size} = $size;
  $r->{vma} = $vma;
  $r->{file_offset} = $file_offset;

  return $r;
}

# Parse text section header of a library using otool (on OS X)
sub ParseTextSectionHeaderFromOtool {
  my $lib = shift;

  my $size = undef;
  my $vma = undef;
  my $file_offset = undef;
  # Get otool output from the library file to figure out how to
  # map between mapped addresses and addresses in the library.
  my $command = ShellEscape($obj_tool_map{"otool"}, "-l", $lib);
  open(OTOOL, "$command |") || error("$command: $!\n");
  my $cmd = "";
  my $sectname = "";
  my $segname = "";
  foreach my $line (<OTOOL>) {
    $line =~ s/\r//g;      # turn windows-looking lines into unix-looking lines
    # Load command <#>
    #       cmd LC_SEGMENT
    # [...]
    # Section
    #   sectname __text
    #    segname __TEXT
    #       addr 0x000009f8
    #       size 0x00018b9e
    #     offset 2552
    #      align 2^2 (4)
    # We will need to strip off the leading 0x from the hex addresses,
    # and convert the offset into hex.
    if ($line =~ /Load command/) {
      $cmd = "";
      $sectname = "";
      $segname = "";
    } elsif ($line =~ /Section/) {
      $sectname = "";
      $segname = "";
    } elsif ($line =~ /cmd (\w+)/) {
      $cmd = $1;
    } elsif ($line =~ /sectname (\w+)/) {
      $sectname = $1;
    } elsif ($line =~ /segname (\w+)/) {
      $segname = $1;
    } elsif (!(($cmd eq "LC_SEGMENT" || $cmd eq "LC_SEGMENT_64") &&
               $sectname eq "__text" &&
               $segname eq "__TEXT")) {
      next;
    } elsif ($line =~ /\baddr 0x([0-9a-fA-F]+)/) {
      $vma = $1;
    } elsif ($line =~ /\bsize 0x([0-9a-fA-F]+)/) {
      $size = $1;
    } elsif ($line =~ /\boffset ([0-9]+)/) {
      $file_offset = sprintf("%016x", $1);
    }
    if (defined($vma) && defined($size) && defined($file_offset)) {
      last;
    }
  }
  close(OTOOL);

  if (!defined($vma) || !defined($size) || !defined($file_offset)) {
     return undef;
  }

  my $r = {};
  $r->{size} = $size;
  $r->{vma} = $vma;
  $r->{file_offset} = $file_offset;

  return $r;
}

sub ParseTextSectionHeader {
  # obj_tool_map("otool") is only defined if we're in a Mach-O environment
  if (defined($obj_tool_map{"otool"})) {
    my $r = ParseTextSectionHeaderFromOtool(@_);
    if (defined($r)){
      return $r;
    }
  }
  # If otool doesn't work, or we don't have it, fall back to objdump
  return ParseTextSectionHeaderFromObjdump(@_);
}

# Split /proc/pid/maps dump into a list of libraries
sub ParseLibraries {
  return if $main::use_symbol_page;  # We don't need libraries info.
  my $prog = Cwd::abs_path(shift);
  my $map = shift;
  my $pcs = shift;

  my $result = [];
  my $h = "[a-f0-9]+";
  my $zero_offset = HexExtend("0");

  my $buildvar = "";
  foreach my $l (split("\n", $map)) {
    if ($l =~ m/^\s*build=(.*)$/) {
      $buildvar = $1;
    }

    my $start;
    my $finish;
    my $offset;
    my $lib;
    if ($l =~ /^($h)-($h)\s+..x.\s+($h)\s+\S+:\S+\s+\d+\s+(.+\.(so|dll|dylib|bundle|node)((\.\d+)+\w*(\.\d+){0,3})?)$/i) {
      # Full line from /proc/self/maps.  Example:
      #   40000000-40015000 r-xp 00000000 03:01 12845071   /lib/ld-2.3.2.so
      $start = HexExtend($1);
      $finish = HexExtend($2);
      $offset = HexExtend($3);
      $lib = $4;
      $lib =~ s|\\|/|g;     # turn windows-style paths into unix-style paths
    } elsif ($l =~ /^\s*($h)-($h):\s*(\S+\.so(\.\d+)*)/) {
      # Cooked line from DumpAddressMap.  Example:
      #   40000000-40015000: /lib/ld-2.3.2.so
      $start = HexExtend($1);
      $finish = HexExtend($2);
      $offset = $zero_offset;
      $lib = $3;
    } elsif (($l =~ /^($h)-($h)\s+..x.\s+($h)\s+\S+:\S+\s+\d+\s+(\S+)$/i) && ($4 eq $prog)) {
      # PIEs and address space randomization do not play well with our
      # default assumption that main executable is at lowest
      # addresses. So we're detecting main executable in
      # /proc/self/maps as well.
      $start = HexExtend($1);
      $finish = HexExtend($2);
      $offset = HexExtend($3);
      $lib = $4;
      $lib =~ s|\\|/|g;     # turn windows-style paths into unix-style paths
    } else {
      next;
    }

    # Expand "$build" variable if available
    $lib =~ s/\$build\b/$buildvar/g;

    $lib = FindLibrary($lib);

    # Check for pre-relocated libraries, which use pre-relocated symbol tables
    # and thus require adjusting the offset that we'll use to translate
    # VM addresses into symbol table addresses.
    # Only do this if we're not going to fetch the symbol table from a
    # debugging copy of the library.
    if (!DebuggingLibrary($lib)) {
      my $text = ParseTextSectionHeader($lib);
      if (defined($text)) {
         my $vma_offset = AddressSub($text->{vma}, $text->{file_offset});
         $offset = AddressAdd($offset, $vma_offset);
      }
    }

    push(@{$result}, [$lib, $start, $finish, $offset]);
  }

  # Append special entry for additional library (not relocated)
  if ($main::opt_lib ne "") {
    my $text = ParseTextSectionHeader($main::opt_lib);
    if (defined($text)) {
       my $start = $text->{vma};
       my $finish = AddressAdd($start, $text->{size});

       push(@{$result}, [$main::opt_lib, $start, $finish, $start]);
    }
  }

  # Append special entry for the main program.  This covers
  # 0..max_pc_value_seen, so that we assume pc values not found in one
  # of the library ranges will be treated as coming from the main
  # program binary.
  my $min_pc = HexExtend("0");
  my $max_pc = $min_pc;          # find the maximal PC value in any sample
  foreach my $pc (keys(%{$pcs})) {
    if (HexExtend($pc) gt $max_pc) { $max_pc = HexExtend($pc); }
  }
  push(@{$result}, [$prog, $min_pc, $max_pc, $zero_offset]);

  return $result;
}

# Add two hex addresses of length $address_length.
# Run pprof --test for unit test if this is changed.
sub AddressAdd {
  my $addr1 = shift;
  my $addr2 = shift;
  my $sum;

  if ($address_length == 8) {
    # Perl doesn't cope with wraparound arithmetic, so do it explicitly:
    $sum = (hex($addr1)+hex($addr2)) % (0x10000000 * 16);
    return sprintf("%08x", $sum);

  } else {
    # Do the addition in 7-nibble chunks to trivialize carry handling.

    if ($main::opt_debug and $main::opt_test) {
      print STDERR "AddressAdd $addr1 + $addr2 = ";
    }

    my $a1 = substr($addr1,-7);
    $addr1 = substr($addr1,0,-7);
    my $a2 = substr($addr2,-7);
    $addr2 = substr($addr2,0,-7);
    $sum = hex($a1) + hex($a2);
    my $c = 0;
    if ($sum > 0xfffffff) {
      $c = 1;
      $sum -= 0x10000000;
    }
    my $r = sprintf("%07x", $sum);

    $a1 = substr($addr1,-7);
    $addr1 = substr($addr1,0,-7);
    $a2 = substr($addr2,-7);
    $addr2 = substr($addr2,0,-7);
    $sum = hex($a1) + hex($a2) + $c;
    $c = 0;
    if ($sum > 0xfffffff) {
      $c = 1;
      $sum -= 0x10000000;
    }
    $r = sprintf("%07x", $sum) . $r;

    $sum = hex($addr1) + hex($addr2) + $c;
    if ($sum > 0xff) { $sum -= 0x100; }
    $r = sprintf("%02x", $sum) . $r;

    if ($main::opt_debug and $main::opt_test) { print STDERR "$r\n"; }

    return $r;
  }
}


# Subtract two hex addresses of length $address_length.
# Run pprof --test for unit test if this is changed.
sub AddressSub {
  my $addr1 = shift;
  my $addr2 = shift;
  my $diff;

  if ($address_length == 8) {
    # Perl doesn't cope with wraparound arithmetic, so do it explicitly:
    $diff = (hex($addr1)-hex($addr2)) % (0x10000000 * 16);
    return sprintf("%08x", $diff);

  } else {
    # Do the addition in 7-nibble chunks to trivialize borrow handling.
    # if ($main::opt_debug) { print STDERR "AddressSub $addr1 - $addr2 = "; }

    my $a1 = hex(substr($addr1,-7));
    $addr1 = substr($addr1,0,-7);
    my $a2 = hex(substr($addr2,-7));
    $addr2 = substr($addr2,0,-7);
    my $b = 0;
    if ($a2 > $a1) {
      $b = 1;
      $a1 += 0x10000000;
    }
    $diff = $a1 - $a2;
    my $r = sprintf("%07x", $diff);

    $a1 = hex(substr($addr1,-7));
    $addr1 = substr($addr1,0,-7);
    $a2 = hex(substr($addr2,-7)) + $b;
    $addr2 = substr($addr2,0,-7);
    $b = 0;
    if ($a2 > $a1) {
      $b = 1;
      $a1 += 0x10000000;
    }
    $diff = $a1 - $a2;
    $r = sprintf("%07x", $diff) . $r;

    $a1 = hex($addr1);
    $a2 = hex($addr2) + $b;
    if ($a2 > $a1) { $a1 += 0x100; }
    $diff = $a1 - $a2;
    $r = sprintf("%02x", $diff) . $r;

    # if ($main::opt_debug) { print STDERR "$r\n"; }

    return $r;
  }
}

# Increment a hex addresses of length $address_length.
# Run pprof --test for unit test if this is changed.
sub AddressInc {
  my $addr = shift;
  my $sum;

  if ($address_length == 8) {
    # Perl doesn't cope with wraparound arithmetic, so do it explicitly:
    $sum = (hex($addr)+1) % (0x10000000 * 16);
    return sprintf("%08x", $sum);

  } else {
    # Do the addition in 7-nibble chunks to trivialize carry handling.
    # We are always doing this to step through the addresses in a function,
    # and will almost never overflow the first chunk, so we check for this
    # case and exit early.

    # if ($main::opt_debug) { print STDERR "AddressInc $addr1 = "; }

    my $a1 = substr($addr,-7);
    $addr = substr($addr,0,-7);
    $sum = hex($a1) + 1;
    my $r = sprintf("%07x", $sum);
    if ($sum <= 0xfffffff) {
      $r = $addr . $r;
      # if ($main::opt_debug) { print STDERR "$r\n"; }
      return HexExtend($r);
    } else {
      $r = "0000000";
    }

    $a1 = substr($addr,-7);
    $addr = substr($addr,0,-7);
    $sum = hex($a1) + 1;
    $r = sprintf("%07x", $sum) . $r;
    if ($sum <= 0xfffffff) {
      $r = $addr . $r;
      # if ($main::opt_debug) { print STDERR "$r\n"; }
      return HexExtend($r);
    } else {
      $r = "00000000000000";
    }

    $sum = hex($addr) + 1;
    if ($sum > 0xff) { $sum -= 0x100; }
    $r = sprintf("%02x", $sum) . $r;

    # if ($main::opt_debug) { print STDERR "$r\n"; }
    return $r;
  }
}

# Extract symbols for all PC values found in profile
sub ExtractSymbols {
  my $libs = shift;
  my $pcset = shift;

  my $symbols = {};

  # Map each PC value to the containing library.  To make this faster,
  # we sort libraries by their starting pc value (highest first), and
  # advance through the libraries as we advance the pc.  Sometimes the
  # addresses of libraries may overlap with the addresses of the main
  # binary, so to make sure the libraries 'win', we iterate over the
  # libraries in reverse order (which assumes the binary doesn't start
  # in the middle of a library, which seems a fair assumption).
  my @pcs = (sort { $a cmp $b } keys(%{$pcset}));  # pcset is 0-extended strings
  foreach my $lib (sort {$b->[1] cmp $a->[1]} @{$libs}) {
    my $libname = $lib->[0];
    my $start = $lib->[1];
    my $finish = $lib->[2];
    my $offset = $lib->[3];

    # Get list of pcs that belong in this library.
    my $contained = [];
    my ($start_pc_index, $finish_pc_index);
    # Find smallest finish_pc_index such that $finish < $pc[$finish_pc_index].
    for ($finish_pc_index = $#pcs + 1; $finish_pc_index > 0;
         $finish_pc_index--) {
      last if $pcs[$finish_pc_index - 1] le $finish;
    }
    # Find smallest start_pc_index such that $start <= $pc[$start_pc_index].
    for ($start_pc_index = $finish_pc_index; $start_pc_index > 0;
         $start_pc_index--) {
      last if $pcs[$start_pc_index - 1] lt $start;
    }
    # This keeps PC values higher than $pc[$finish_pc_index] in @pcs,
    # in case there are overlaps in libraries and the main binary.
    @{$contained} = splice(@pcs, $start_pc_index,
                           $finish_pc_index - $start_pc_index);
    # Map to symbols
    MapToSymbols($libname, AddressSub($start, $offset), $contained, $symbols);
  }

  return $symbols;
}

# Map list of PC values to symbols for a given image
sub MapToSymbols {
  my $image = shift;
  my $offset = shift;
  my $pclist = shift;
  my $symbols = shift;

  my $debug = 0;

  # For libc (and other) libraries, the copy in /usr/lib/debug contains debugging symbols
  my $debugging = DebuggingLibrary($image);
  if ($debugging) {
    $image = $debugging;
  }

  # Ignore empty binaries
  if ($#{$pclist} < 0) { return; }

  # Figure out the addr2line command to use
  my $addr2line = $obj_tool_map{"addr2line"};
  my $cmd = ShellEscape($addr2line, "-f", "-C", "-e", $image);
  if (exists $obj_tool_map{"addr2line_pdb"}) {
    $addr2line = $obj_tool_map{"addr2line_pdb"};
    $cmd = ShellEscape($addr2line, "--demangle", "-f", "-C", "-e", $image);
  }

  # If "addr2line" isn't installed on the system at all, just use
  # nm to get what info we can (function names, but not line numbers).
  if (system(ShellEscape($addr2line, "--help") . " >$dev_null 2>&1") != 0) {
    MapSymbolsWithNM($image, $offset, $pclist, $symbols);
    return;
  }

  # "addr2line -i" can produce a variable number of lines per input
  # address, with no separator that allows us to tell when data for
  # the next address starts.  So we find the address for a special
  # symbol (_fini) and interleave this address between all real
  # addresses passed to addr2line.  The name of this special symbol
  # can then be used as a separator.
  $sep_address = undef;  # May be filled in by MapSymbolsWithNM()
  my $nm_symbols = {};
  MapSymbolsWithNM($image, $offset, $pclist, $nm_symbols);
  if (defined($sep_address)) {
    # Only add " -i" to addr2line if the binary supports it.
    # addr2line --help returns 0, but not if it sees an unknown flag first.
    if (system("$cmd -i --help >$dev_null 2>&1") == 0) {
      $cmd .= " -i";
    } else {
      $sep_address = undef;   # no need for sep_address if we don't support -i
    }
  }

  # Make file with all PC values with intervening 'sep_address' so
  # that we can reliably detect the end of inlined function list
  open(ADDRESSES, ">$main::tmpfile_sym") || error("$main::tmpfile_sym: $!\n");
  if ($debug) { print("---- $image ---\n"); }
  for (my $i = 0; $i <= $#{$pclist}; $i++) {
    # addr2line always reads hex addresses, and does not need '0x' prefix.
    if ($debug) { printf STDERR ("%s\n", $pclist->[$i]); }
    printf ADDRESSES ("%s\n", AddressSub($pclist->[$i], $offset));
    if (defined($sep_address)) {
      printf ADDRESSES ("%s\n", $sep_address);
    }
  }
  close(ADDRESSES);
  if ($debug) {
    print("----\n");
    system("cat", $main::tmpfile_sym);
    print("---- $cmd ---\n");
    system("$cmd < " . ShellEscape($main::tmpfile_sym));
    print("----\n");
  }

  open(SYMBOLS, "$cmd <" . ShellEscape($main::tmpfile_sym) . " |")
      || error("$cmd: $!\n");
  my $count = 0;   # Index in pclist
  while (<SYMBOLS>) {
    # Read fullfunction and filelineinfo from next pair of lines
    s/\r?\n$//g;
    my $fullfunction = $_;
    $_ = <SYMBOLS>;
    s/\r?\n$//g;
    my $filelinenum = $_;

    if (defined($sep_address) && $fullfunction eq $sep_symbol) {
      # Terminating marker for data for this address
      $count++;
      next;
    }

    $filelinenum =~ s|\\|/|g; # turn windows-style paths into unix-style paths

    # Remove discriminator markers as this comes after the line number and
    # confuses the rest of this script.
    $filelinenum =~ s/ \(discriminator \d+\)$//;
    # Convert unknown line numbers into line 0.
    $filelinenum =~ s/:\?$/:0/;

    my $pcstr = $pclist->[$count];
    my $function = ShortFunctionName($fullfunction);
    my $nms = $nm_symbols->{$pcstr};
    if (defined($nms)) {
      if ($fullfunction eq '??') {
        # nm found a symbol for us.
        $function = $nms->[0];
        $fullfunction = $nms->[2];
      } else {
	# MapSymbolsWithNM tags each routine with its starting address,
	# useful in case the image has multiple occurrences of this
	# routine.  (It uses a syntax that resembles template paramters,
	# that are automatically stripped out by ShortFunctionName().)
	# addr2line does not provide the same information.  So we check
	# if nm disambiguated our symbol, and if so take the annotated
	# (nm) version of the routine-name.  TODO(csilvers): this won't
	# catch overloaded, inlined symbols, which nm doesn't see.
	# Better would be to do a check similar to nm's, in this fn.
	if ($nms->[2] =~ m/^\Q$function\E/) {  # sanity check it's the right fn
	  $function = $nms->[0];
	  $fullfunction = $nms->[2];
	}
      }
    }
    
    # Prepend to accumulated symbols for pcstr
    # (so that caller comes before callee)
    my $sym = $symbols->{$pcstr};
    if (!defined($sym)) {
      $sym = [];
      $symbols->{$pcstr} = $sym;
    }
    unshift(@{$sym}, $function, $filelinenum, $fullfunction);
    if ($debug) { printf STDERR ("%s => [%s]\n", $pcstr, join(" ", @{$sym})); }
    if (!defined($sep_address)) {
      # Inlining is off, so this entry ends immediately
      $count++;
    }
  }
  close(SYMBOLS);
}

# Use nm to map the list of referenced PCs to symbols.  Return true iff we
# are able to read procedure information via nm.
sub MapSymbolsWithNM {
  my $image = shift;
  my $offset = shift;
  my $pclist = shift;
  my $symbols = shift;

  # Get nm output sorted by increasing address
  my $symbol_table = GetProcedureBoundaries($image, ".");
  if (!%{$symbol_table}) {
    return 0;
  }
  # Start addresses are already the right length (8 or 16 hex digits).
  my @names = sort { $symbol_table->{$a}->[0] cmp $symbol_table->{$b}->[0] }
    keys(%{$symbol_table});

  if ($#names < 0) {
    # No symbols: just use addresses
    foreach my $pc (@{$pclist}) {
      my $pcstr = "0x" . $pc;
      $symbols->{$pc} = [$pcstr, "?", $pcstr];
    }
    return 0;
  }

  # Sort addresses so we can do a join against nm output
  my $index = 0;
  my $fullname = $names[0];
  my $name = ShortFunctionName($fullname);
  foreach my $pc (sort { $a cmp $b } @{$pclist}) {
    # Adjust for mapped offset
    my $mpc = AddressSub($pc, $offset);
    while (($index < $#names) && ($mpc ge $symbol_table->{$fullname}->[1])){
      $index++;
      $fullname = $names[$index];
      $name = ShortFunctionName($fullname);
    }
    if ($mpc lt $symbol_table->{$fullname}->[1]) {
      $symbols->{$pc} = [$name, "?", $fullname];
    } else {
      my $pcstr = "0x" . $pc;
      $symbols->{$pc} = [$pcstr, "?", $pcstr];
    }
  }
  return 1;
}

sub ShortFunctionName {
  my $function = shift;
  while ($function =~ s/\([^()]*\)(\s*const)?//g) { }   # Argument types
  $function =~ s/<[0-9a-f]*>$//g;                # Remove Address
  if (!$main::opt_no_strip_temp) {
      while ($function =~ s/<[^<>]*>//g)  { }   # Remove template arguments
  }
  $function =~ s/^.*\s+(\w+::)/$1/;          # Remove leading type
  return $function;
}

# Trim overly long symbols found in disassembler output
sub CleanDisassembly {
  my $d = shift;
  while ($d =~ s/\([^()%]*\)(\s*const)?//g) { } # Argument types, not (%rax)
  while ($d =~ s/(\w+)<[^<>]*>/$1/g)  { }       # Remove template arguments
  return $d;
}

# Clean file name for display
sub CleanFileName {
  my ($f) = @_;
  $f =~ s|^/proc/self/cwd/||;
  $f =~ s|^\./||;
  return $f;
}

# Make address relative to section and clean up for display
sub UnparseAddress {
  my ($offset, $address) = @_;
  $address = AddressSub($address, $offset);
  $address =~ s/^0x//;
  $address =~ s/^0*//;
  return $address;
}

##### Miscellaneous #####

# Find the right versions of the above object tools to use.  The
# argument is the program file being analyzed, and should be an ELF
# 32-bit or ELF 64-bit executable file.  The location of the tools
# is determined by considering the following options in this order:
#   1) --tools option, if set
#   2) PPROF_TOOLS environment variable, if set
#   3) the environment
sub ConfigureObjTools {
  my $prog_file = shift;

  # Check for the existence of $prog_file because /usr/bin/file does not
  # predictably return error status in prod.
  (-e $prog_file)  || error("$prog_file does not exist.\n");

  my $file_type = undef;
  if (-e "/usr/bin/file") {
    # Follow symlinks (at least for systems where "file" supports that).
    my $escaped_prog_file = ShellEscape($prog_file);
    $file_type = `/usr/bin/file -L $escaped_prog_file 2>$dev_null ||
                  /usr/bin/file $escaped_prog_file`;
  } elsif ($^O == "MSWin32") {
    $file_type = "MS Windows";
  } else {
    print STDERR "WARNING: Can't determine the file type of $prog_file";
  }

  if ($file_type =~ /64-bit/) {
    # Change $address_length to 16 if the program file is ELF 64-bit.
    # We can't detect this from many (most?) heap or lock contention
    # profiles, since the actual addresses referenced are generally in low
    # memory even for 64-bit programs.
    $address_length = 16;
  }

  if ($file_type =~ /MS Windows/) {
    # For windows, we provide a version of nm and addr2line as part of
    # the opensource release, which is capable of parsing
    # Windows-style PDB executables.  It should live in the path, or
    # in the same directory as pprof.
    $obj_tool_map{"nm_pdb"} = "nm-pdb";
    $obj_tool_map{"addr2line_pdb"} = "addr2line-pdb";
  }

  if ($file_type =~ /Mach-O/) {
    # OS X uses otool to examine Mach-O files, rather than objdump.
    $obj_tool_map{"otool"} = "otool";
    $obj_tool_map{"addr2line"} = "false";  # no addr2line
    $obj_tool_map{"objdump"} = "false";  # no objdump
  }

  # Go fill in %obj_tool_map with the pathnames to use:
  foreach my $tool (keys %obj_tool_map) {
    $obj_tool_map{$tool} = ConfigureTool($obj_tool_map{$tool});
  }
}

# Returns the path of a caller-specified object tool.  If --tools or
# PPROF_TOOLS are specified, then returns the full path to the tool
# with that prefix.  Otherwise, returns the path unmodified (which
# means we will look for it on PATH).
sub ConfigureTool {
  my $tool = shift;
  my $path;

  # --tools (or $PPROF_TOOLS) is a comma separated list, where each
  # item is either a) a pathname prefix, or b) a map of the form
  # <tool>:<path>.  First we look for an entry of type (b) for our
  # tool.  If one is found, we use it.  Otherwise, we consider all the
  # pathname prefixes in turn, until one yields an existing file.  If
  # none does, we use a default path.
  my $tools = $main::opt_tools || $ENV{"PPROF_TOOLS"} || "";
  if ($tools =~ m/(,|^)\Q$tool\E:([^,]*)/) {
    $path = $2;
    # TODO(csilvers): sanity-check that $path exists?  Hard if it's relative.
  } elsif ($tools ne '') {
    foreach my $prefix (split(',', $tools)) {
      next if ($prefix =~ /:/);    # ignore "tool:fullpath" entries in the list
      if (-x $prefix . $tool) {
        $path = $prefix . $tool;
        last;
      }
    }
    if (!$path) {
      error("No '$tool' found with prefix specified by " .
            "--tools (or \$PPROF_TOOLS) '$tools'\n");
    }
  } else {
    # ... otherwise use the version that exists in the same directory as
    # pprof.  If there's nothing there, use $PATH.
    $0 =~ m,[^/]*$,;     # this is everything after the last slash
    my $dirname = $`;    # this is everything up to and including the last slash
    if (-x "$dirname$tool") {
      $path = "$dirname$tool";
    } else { 
      $path = $tool;
    }
  }
  if ($main::opt_debug) { print STDERR "Using '$path' for '$tool'.\n"; }
  return $path;
}

sub ShellEscape {
  my @escaped_words = ();
  foreach my $word (@_) {
    my $escaped_word = $word;
    if ($word =~ m![^a-zA-Z0-9/.,_=-]!) {  # check for anything not in whitelist
      $escaped_word =~ s/'/'\\''/;
      $escaped_word = "'$escaped_word'";
    }
    push(@escaped_words, $escaped_word);
  }
  return join(" ", @escaped_words);
}

sub cleanup {
  unlink($main::tmpfile_sym);
  unlink(keys %main::tempnames);

  # We leave any collected profiles in $HOME/pprof in case the user wants
  # to look at them later.  We print a message informing them of this.
  if ((scalar(@main::profile_files) > 0) &&
      defined($main::collected_profile)) {
    if (scalar(@main::profile_files) == 1) {
      print STDERR "Dynamically gathered profile is in $main::collected_profile\n";
    }
    print STDERR "If you want to investigate this profile further, you can do:\n";
    print STDERR "\n";
    print STDERR "  $0 \\\n";
    print STDERR "    $main::prog \\\n";
    print STDERR "    $main::collected_profile\n";
    print STDERR "\n";
  }
}

sub sighandler {
  cleanup();
  exit(1);
}

sub error {
  my $msg = shift;
  print STDERR $msg;
  cleanup();
  exit(1);
}


# Run $nm_command and get all the resulting procedure boundaries whose
# names match "$regexp" and returns them in a hashtable mapping from
# procedure name to a two-element vector of [start address, end address]
sub GetProcedureBoundariesViaNm {
  my $escaped_nm_command = shift;    # shell-escaped
  my $regexp = shift;
  my $image = shift;

  my $symbol_table = {};
  open(NM, "$escaped_nm_command |") || error("$escaped_nm_command: $!\n");
  my $last_start = "0";
  my $routine = "";
  while (<NM>) {
    s/\r//g;         # turn windows-looking lines into unix-looking lines
    if (m/^\s*([0-9a-f]+) (.) (..*)/) {
      my $start_val = $1;
      my $type = $2;
      my $this_routine = $3;

      # It's possible for two symbols to share the same address, if
      # one is a zero-length variable (like __start_google_malloc) or
      # one symbol is a weak alias to another (like __libc_malloc).
      # In such cases, we want to ignore all values except for the
      # actual symbol, which in nm-speak has type "T".  The logic
      # below does this, though it's a bit tricky: what happens when
      # we have a series of lines with the same address, is the first
      # one gets queued up to be processed.  However, it won't
      # *actually* be processed until later, when we read a line with
      # a different address.  That means that as long as we're reading
      # lines with the same address, we have a chance to replace that
      # item in the queue, which we do whenever we see a 'T' entry --
      # that is, a line with type 'T'.  If we never see a 'T' entry,
      # we'll just go ahead and process the first entry (which never
      # got touched in the queue), and ignore the others.
      if ($start_val eq $last_start && $type =~ /t/i) {
        # We are the 'T' symbol at this address, replace previous symbol.
        $routine = $this_routine;
        next;
      } elsif ($start_val eq $last_start) {
        # We're not the 'T' symbol at this address, so ignore us.
        next;
      }

      if ($this_routine eq $sep_symbol) {
        $sep_address = HexExtend($start_val);
      }

      # Tag this routine with the starting address in case the image
      # has multiple occurrences of this routine.  We use a syntax
      # that resembles template paramters that are automatically
      # stripped out by ShortFunctionName()
      $this_routine .= "<$start_val>";

      if (defined($routine) && $routine =~ m/$regexp/) {
        $symbol_table->{$routine} = [HexExtend($last_start),
                                     HexExtend($start_val)];
      }
      $last_start = $start_val;
      $routine = $this_routine;
    } elsif (m/^Loaded image name: (.+)/) {
      # The win32 nm workalike emits information about the binary it is using.
      if ($main::opt_debug) { print STDERR "Using Image $1\n"; }
    } elsif (m/^PDB file name: (.+)/) {
      # The win32 nm workalike emits information about the pdb it is using.
      if ($main::opt_debug) { print STDERR "Using PDB $1\n"; }
    }
  }
  close(NM);
  # Handle the last line in the nm output.  Unfortunately, we don't know
  # how big this last symbol is, because we don't know how big the file
  # is.  For now, we just give it a size of 0.
  # TODO(csilvers): do better here.
  if (defined($routine) && $routine =~ m/$regexp/) {
    $symbol_table->{$routine} = [HexExtend($last_start),
                                 HexExtend($last_start)];
  }

  # Verify if addr2line can find the $sep_symbol.  If not, we use objdump
  # to find the address for the $sep_symbol on code section which addr2line
  # can find.
  if (defined($sep_address)){
    my $start_val = $sep_address;
    my $addr2line = $obj_tool_map{"addr2line"};
    my $cmd = ShellEscape($addr2line, "-f", "-C", "-e", $image, "-i");
    open(FINI, "echo $start_val | $cmd  |")
         || error("echo $start_val | $cmd: $!\n");
    $_ = <FINI>;
    s/\r?\n$//g;
    my $fini = $_;
    close(FINI);
    if ($fini ne $sep_symbol){
      my $objdump =  $obj_tool_map{"objdump"};
      $cmd = ShellEscape($objdump, "-d", $image);
      my $grep = ShellEscape("grep", $sep_symbol);
      my $tail = ShellEscape("tail", "-n", "1");
      open(FINI, "$cmd | $grep | $tail |")
           || error("$cmd | $grep | $tail: $!\n");
      s/\r//g; # turn windows-looking lines into unix-looking lines
      my $data = <FINI>;
      if (defined($data)){
        ($start_val, $fini) = split(/ </,$data);
      }
      close(FINI);
    }
    $sep_address = HexExtend($start_val);
  }

  return $symbol_table;
}

# Gets the procedure boundaries for all routines in "$image" whose names
# match "$regexp" and returns them in a hashtable mapping from procedure
# name to a two-element vector of [start address, end address].
# Will return an empty map if nm is not installed or not working properly.
sub GetProcedureBoundaries {
  my $image = shift;
  my $regexp = shift;

  # If $image doesn't start with /, then put ./ in front of it.  This works
  # around an obnoxious bug in our probing of nm -f behavior.
  # "nm -f $image" is supposed to fail on GNU nm, but if:
  #
  # a. $image starts with [BbSsPp] (for example, bin/foo/bar), AND
  # b. you have a.out in your current directory (a not uncommon occurrence)
  #
  # then "nm -f $image" succeeds because -f only looks at the first letter of
  # the argument, which looks valid because it's [BbSsPp], and then since
  # there's no image provided, it looks for a.out and finds it.
  #
  # This regex makes sure that $image starts with . or /, forcing the -f
  # parsing to fail since . and / are not valid formats.
  $image =~ s#^[^/]#./$&#;

  # For libc libraries, the copy in /usr/lib/debug contains debugging symbols
  my $debugging = DebuggingLibrary($image);
  if ($debugging) {
    $image = $debugging;
  }

  my $nm = $obj_tool_map{"nm"};
  my $cppfilt = $obj_tool_map{"c++filt"};

  # nm can fail for two reasons: 1) $image isn't a debug library; 2) nm
  # binary doesn't support --demangle.  In addition, for OS X we need
  # to use the -f flag to get 'flat' nm output (otherwise we don't sort
  # properly and get incorrect results).  Unfortunately, GNU nm uses -f
  # in an incompatible way.  So first we test whether our nm supports
  # --demangle and -f.
  my $demangle_flag = "";
  my $cppfilt_flag = "";
  my $to_devnull = ">$dev_null 2>&1";
  if (system(ShellEscape($nm, "--demangle", $image) . $to_devnull) == 0) {
    # In this mode, we do "nm --demangle <foo>"
    $demangle_flag = "--demangle";
    $cppfilt_flag = "";
  } elsif (system(ShellEscape($cppfilt, $image) . $to_devnull) == 0) {
    # In this mode, we do "nm <foo> | c++filt"
    $cppfilt_flag = " | " . ShellEscape($cppfilt);
  };
  my $flatten_flag = "";
  if (system(ShellEscape($nm, "-f", $image) . $to_devnull) == 0) {
    $flatten_flag = "-f";
  }

  # Finally, in the case $imagie isn't a debug library, we try again with
  # -D to at least get *exported* symbols.  If we can't use --demangle,
  # we use c++filt instead, if it exists on this system.
  my @nm_commands = (ShellEscape($nm, "-n", $flatten_flag, $demangle_flag,
                                 $image) . " 2>$dev_null $cppfilt_flag",
                     ShellEscape($nm, "-D", "-n", $flatten_flag, $demangle_flag,
                                 $image) . " 2>$dev_null $cppfilt_flag",
                     # 6nm is for Go binaries
                     ShellEscape("6nm", "$image") . " 2>$dev_null | sort",
                     );

  # If the executable is an MS Windows PDB-format executable, we'll
  # have set up obj_tool_map("nm_pdb").  In this case, we actually
  # want to use both unix nm and windows-specific nm_pdb, since
  # PDB-format executables can apparently include dwarf .o files.
  if (exists $obj_tool_map{"nm_pdb"}) {
    push(@nm_commands,
         ShellEscape($obj_tool_map{"nm_pdb"}, "--demangle", $image)
         . " 2>$dev_null");
  }

  foreach my $nm_command (@nm_commands) {
    my $symbol_table = GetProcedureBoundariesViaNm($nm_command, $regexp, $image);
    return $symbol_table if (%{$symbol_table});
  }
  my $symbol_table = {};
  return $symbol_table;
}


# The test vectors for AddressAdd/Sub/Inc are 8-16-nibble hex strings.
# To make them more readable, we add underscores at interesting places.
# This routine removes the underscores, producing the canonical representation
# used by pprof to represent addresses, particularly in the tested routines.
sub CanonicalHex {
  my $arg = shift;
  return join '', (split '_',$arg);
}


# Unit test for AddressAdd:
sub AddressAddUnitTest {
  my $test_data_8 = shift;
  my $test_data_16 = shift;
  my $error_count = 0;
  my $fail_count = 0;
  my $pass_count = 0;
  # print STDERR "AddressAddUnitTest: ", 1+$#{$test_data_8}, " tests\n";

  # First a few 8-nibble addresses.  Note that this implementation uses
  # plain old arithmetic, so a quick sanity check along with verifying what
  # happens to overflow (we want it to wrap):
  $address_length = 8;
  foreach my $row (@{$test_data_8}) {
    if ($main::opt_debug and $main::opt_test) { print STDERR "@{$row}\n"; }
    my $sum = AddressAdd ($row->[0], $row->[1]);
    if ($sum ne $row->[2]) {
      printf STDERR "ERROR: %s != %s + %s = %s\n", $sum,
             $row->[0], $row->[1], $row->[2];
      ++$fail_count;
    } else {
      ++$pass_count;
    }
  }
  printf STDERR "AddressAdd 32-bit tests: %d passes, %d failures\n",
         $pass_count, $fail_count;
  $error_count = $fail_count;
  $fail_count = 0;
  $pass_count = 0;

  # Now 16-nibble addresses.
  $address_length = 16;
  foreach my $row (@{$test_data_16}) {
    if ($main::opt_debug and $main::opt_test) { print STDERR "@{$row}\n"; }
    my $sum = AddressAdd (CanonicalHex($row->[0]), CanonicalHex($row->[1]));
    my $expected = join '', (split '_',$row->[2]);
    if ($sum ne CanonicalHex($row->[2])) {
      printf STDERR "ERROR: %s != %s + %s = %s\n", $sum,
             $row->[0], $row->[1], $row->[2];
      ++$fail_count;
    } else {
      ++$pass_count;
    }
  }
  printf STDERR "AddressAdd 64-bit tests: %d passes, %d failures\n",
         $pass_count, $fail_count;
  $error_count += $fail_count;

  return $error_count;
}


# Unit test for AddressSub:
sub AddressSubUnitTest {
  my $test_data_8 = shift;
  my $test_data_16 = shift;
  my $error_count = 0;
  my $fail_count = 0;
  my $pass_count = 0;
  # print STDERR "AddressSubUnitTest: ", 1+$#{$test_data_8}, " tests\n";

  # First a few 8-nibble addresses.  Note that this implementation uses
  # plain old arithmetic, so a quick sanity check along with verifying what
  # happens to overflow (we want it to wrap):
  $address_length = 8;
  foreach my $row (@{$test_data_8}) {
    if ($main::opt_debug and $main::opt_test) { print STDERR "@{$row}\n"; }
    my $sum = AddressSub ($row->[0], $row->[1]);
    if ($sum ne $row->[3]) {
      printf STDERR "ERROR: %s != %s - %s = %s\n", $sum,
             $row->[0], $row->[1], $row->[3];
      ++$fail_count;
    } else {
      ++$pass_count;
    }
  }
  printf STDERR "AddressSub 32-bit tests: %d passes, %d failures\n",
         $pass_count, $fail_count;
  $error_count = $fail_count;
  $fail_count = 0;
  $pass_count = 0;

  # Now 16-nibble addresses.
  $address_length = 16;
  foreach my $row (@{$test_data_16}) {
    if ($main::opt_debug and $main::opt_test) { print STDERR "@{$row}\n"; }
    my $sum = AddressSub (CanonicalHex($row->[0]), CanonicalHex($row->[1]));
    if ($sum ne CanonicalHex($row->[3])) {
      printf STDERR "ERROR: %s != %s - %s = %s\n", $sum,
             $row->[0], $row->[1], $row->[3];
      ++$fail_count;
    } else {
      ++$pass_count;
    }
  }
  printf STDERR "AddressSub 64-bit tests: %d passes, %d failures\n",
         $pass_count, $fail_count;
  $error_count += $fail_count;

  return $error_count;
}


# Unit test for AddressInc:
sub AddressIncUnitTest {
  my $test_data_8 = shift;
  my $test_data_16 = shift;
  my $error_count = 0;
  my $fail_count = 0;
  my $pass_count = 0;
  # print STDERR "AddressIncUnitTest: ", 1+$#{$test_data_8}, " tests\n";

  # First a few 8-nibble addresses.  Note that this implementation uses
  # plain old arithmetic, so a quick sanity check along with verifying what
  # happens to overflow (we want it to wrap):
  $address_length = 8;
  foreach my $row (@{$test_data_8}) {
    if ($main::opt_debug and $main::opt_test) { print STDERR "@{$row}\n"; }
    my $sum = AddressInc ($row->[0]);
    if ($sum ne $row->[4]) {
      printf STDERR "ERROR: %s != %s + 1 = %s\n", $sum,
             $row->[0], $row->[4];
      ++$fail_count;
    } else {
      ++$pass_count;
    }
  }
  printf STDERR "AddressInc 32-bit tests: %d passes, %d failures\n",
         $pass_count, $fail_count;
  $error_count = $fail_count;
  $fail_count = 0;
  $pass_count = 0;

  # Now 16-nibble addresses.
  $address_length = 16;
  foreach my $row (@{$test_data_16}) {
    if ($main::opt_debug and $main::opt_test) { print STDERR "@{$row}\n"; }
    my $sum = AddressInc (CanonicalHex($row->[0]));
    if ($sum ne CanonicalHex($row->[4])) {
      printf STDERR "ERROR: %s != %s + 1 = %s\n", $sum,
             $row->[0], $row->[4];
      ++$fail_count;
    } else {
      ++$pass_count;
    }
  }
  printf STDERR "AddressInc 64-bit tests: %d passes, %d failures\n",
         $pass_count, $fail_count;
  $error_count += $fail_count;

  return $error_count;
}


# Driver for unit tests.
# Currently just the address add/subtract/increment routines for 64-bit.
sub RunUnitTests {
  my $error_count = 0;

  # This is a list of tuples [a, b, a+b, a-b, a+1]
  my $unit_test_data_8 = [
    [qw(aaaaaaaa 50505050 fafafafa 5a5a5a5a aaaaaaab)],
    [qw(50505050 aaaaaaaa fafafafa a5a5a5a6 50505051)],
    [qw(ffffffff aaaaaaaa aaaaaaa9 55555555 00000000)],
    [qw(00000001 ffffffff 00000000 00000002 00000002)],
    [qw(00000001 fffffff0 fffffff1 00000011 00000002)],
  ];
  my $unit_test_data_16 = [
    # The implementation handles data in 7-nibble chunks, so those are the
    # interesting boundaries.
    [qw(aaaaaaaa 50505050
        00_000000f_afafafa 00_0000005_a5a5a5a 00_000000a_aaaaaab)],
    [qw(50505050 aaaaaaaa
        00_000000f_afafafa ff_ffffffa_5a5a5a6 00_0000005_0505051)],
    [qw(ffffffff aaaaaaaa
        00_000001a_aaaaaa9 00_0000005_5555555 00_0000010_0000000)],
    [qw(00000001 ffffffff
        00_0000010_0000000 ff_ffffff0_0000002 00_0000000_0000002)],
    [qw(00000001 fffffff0
        00_000000f_ffffff1 ff_ffffff0_0000011 00_0000000_0000002)],

    [qw(00_a00000a_aaaaaaa 50505050
        00_a00000f_afafafa 00_a000005_a5a5a5a 00_a00000a_aaaaaab)],
    [qw(0f_fff0005_0505050 aaaaaaaa
        0f_fff000f_afafafa 0f_ffefffa_5a5a5a6 0f_fff0005_0505051)],
    [qw(00_000000f_fffffff 01_800000a_aaaaaaa
        01_800001a_aaaaaa9 fe_8000005_5555555 00_0000010_0000000)],
    [qw(00_0000000_0000001 ff_fffffff_fffffff
        00_0000000_0000000 00_0000000_0000002 00_0000000_0000002)],
    [qw(00_0000000_0000001 ff_fffffff_ffffff0
        ff_fffffff_ffffff1 00_0000000_0000011 00_0000000_0000002)],
  ];

  $error_count += AddressAddUnitTest($unit_test_data_8, $unit_test_data_16);
  $error_count += AddressSubUnitTest($unit_test_data_8, $unit_test_data_16);
  $error_count += AddressIncUnitTest($unit_test_data_8, $unit_test_data_16);
  if ($error_count > 0) {
    print STDERR $error_count, " errors: FAILED\n";
  } else {
    print STDERR "PASS\n";
  }
  exit ($error_count);
}
