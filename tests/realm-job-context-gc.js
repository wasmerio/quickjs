import { assert } from "./assert.js";

// Regression test: a queued job keeps the realm it was queued from alive until
// it runs.
//
// A promise of this realm is resolved from another realm, so its reaction job
// is queued under that other realm. Nothing else refers to the other realm, so
// the collector frees it before the job runs unless the job holds a reference
// to it.

let got;
let resolve;
new Promise((r) => { resolve = r; }).then((v) => { got = v; });

(function () {
    const other = $262.createRealm();
    other.global.resolve = resolve;
    other.evalScript("resolve(5); delete globalThis.resolve;");
})();

$262.gc();

// Queued after the reaction job, so it runs once that job has run.
Promise.resolve().then(() => assert(got, 5));
