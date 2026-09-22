import { assert } from "./assert.js";

// Keep the fork's lazy prepareStackTrace contract when using upstream's
// Error.prototype.stack accessor and internal Error stack storage.
let calls = 0;
Error.prepareStackTrace = (error, frames) => {
    calls++;
    assert(frames.length > 0);
    return `prepared:${error.message}`;
};

const error = new Error("lazy");
assert(calls, 0);
assert(error.stack, "prepared:lazy");
assert(error.stack, "prepared:lazy");
assert(calls, 1);

const target = { message: "captured" };
Error.captureStackTrace(target);
assert(calls, 1);
assert(target.stack, "prepared:captured");
assert(target.stack, "prepared:captured");
assert(calls, 2);

const assigned = new Error("assigned");
assigned.stack = "override";
assert(assigned.stack, "override");
assert(calls, 2);

Error.prepareStackTrace = undefined;
const ordinary = new Error("ordinary");
const getter = Object.getOwnPropertyDescriptor(Error.prototype, "stack").get;
assert(getter.call(ordinary).includes("fork-lazy-stack.js"));
assert(!Object.hasOwn(ordinary, "stack"));
