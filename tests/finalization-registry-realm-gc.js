// Regression test: a FinalizationRegistry keeps the realm whose constructor
// made it alive.
//
// Nothing else of the other realm stays reachable: the cleanup callback comes
// from this realm and the registry's prototype is removed. Registering a target
// afterwards still uses the registry's realm.

let registry;
(function () {
    const other = $262.createRealm();
    registry = new other.global.FinalizationRegistry(() => {});
    Object.setPrototypeOf(registry, null);
})();

$262.gc();

FinalizationRegistry.prototype.register.call(registry, {}, "held");

// The target is unreachable now: its cleanup job is queued in the registry's
// realm and runs after this script.
$262.gc();
