# IDLE-GC

"The Devil's hands are idle playthings."
                    -- Futurama, S04E18

## RATIONALE

This module runs the garbage collector at times when node.js is otherwise idle.

It is a replacement for the built-in functionality that is scheduled for
removal in node.js v0.10.

The reasons for removing it from node.js core are twofold:

1. The implementation has severe deficiencies.  Many people have reported
   issues where their otherwise idle node.js server uses 100% CPU, trying
   to collect (often non-existent) garbage.

2. The garbage collector has much improved.  In the old days, forcing the
   garbage collector to run at opportune times could significantly increase
   throughput.  The current incremental collector however is _much_ better,
   so much so that in most cases the idle GC scheme is superfluous at best
   and possibly a deoptimization.

This module attempts to fix the deficiencies while preserving the functionality
for people that need it.  Reasons for using it include:

1. Low latency.  If you have an application that needs low latency, this module
   can help amortize the overhead of the garbage collector - but only if your
   application is sufficiently idle.

2. Reducing memory usage.  The garbage collector trades space for time; given
   the chance, it will allocate more heap memory if that means it won't have
   to scan the existing heap as often.

   That's almost always a worthwhile trade-off unless your application runs in
   a restricted environment (say a budget VPS with only 256 or 512 MB of RAM.)
   where the larger memory footprint is actually detrimental.

## USAGE

The module exports two functions, `start()` and `stop()`.

    var g = require('idle-gc');
    g.start();      // Run at 5 second intervals.
    g.start(7500);  // Run at 7.5 second intervals. Stops the old timer first.
    g.stop();       // Stop the timer.

The default interval is 5 seconds.  That means the first GC run starts 5 seconds
after the last activity.

'Activity' in this context means 'any operation that somehow causes the event
loop to move forward', be that file or network I/O, a JavaScript timer, etc.

If there is no more garbage to collect, the timer disables itself.
It is automatically re-enabled when new activity happens (unless explicitly
stopped, of course.)

## CAVEATS

As mentioned in the USAGE section, timers are considered activity. It therefore
follows that in this example the idle GC never runs because the JavaScript timer
pre-empts it every time.

    var g = require('idle-gc');
    g.start(2500);         // Run at 2.5 second intervals.
    setInterval(f, 2000);  // Run at 2.0 second intervals.
    function f() {}
