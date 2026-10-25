// Regression test (bellard/quickjs#280): a module whose top-level await never
// settles is released when its context is freed.
//
// The imported module is still pending when this test's context and runtime
// are freed. The functions that would resume it hold a reference to it, so the
// module must outlive the context's list of loaded modules rather than be
// freed with it while they still point at it.

import("./fixture_pending_tla.js");
