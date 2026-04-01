```
Make the following changes to `PalletRegistry.h`. For each change, the reasoning
is provided so you understand the intent — do not deviate from it.

---

1. INTRODUCE a `PalletID` enum class to replace raw string keys.

   Reason: the current `lookup(const std::string& key)` API accepts arbitrary
   strings, meaning any typo at a call site compiles silently and throws
   `std::out_of_range` at runtime. An enum makes every invalid lookup a
   compile-time error with zero runtime cost.

   Add this enum in `Types.h` (or a new `PalletID.h` if Types.h is not the
   right home for it — check what is already there before deciding):

       enum class PalletID {
           EPAL_EUR_1,
           NA_GMA_STRINGER_48x40,
           AU_AS4068_1165x1165,
           // one enumerator per entry that exists in the registry
       };

   Add one enumerator for every pallet key currently documented or used in
   the codebase. Search all call sites of `PalletRegistry::lookup` to find
   every key string in use and ensure each one has a corresponding enumerator.

---

2. CHANGE the `Registry` alias and `lookup` signature to use `PalletID`.

   Replace:
       using Registry = std::unordered_map<std::string, PalletSpec>;
       [[nodiscard]] const PalletSpec& lookup(const std::string& key);

   With:
       using Registry = std::unordered_map<PalletID, PalletSpec>;
       [[nodiscard]] const PalletSpec& lookup(PalletID id);

   You will need to provide a hash specialization for `PalletID` since
   `std::unordered_map` has no default hash for enum classes. Add this
   before the namespace, or in a dedicated hash header:

       namespace std {
           template <>
           struct hash<PalletID> {
               size_t operator()(PalletID id) const noexcept {
                   return std::hash<int>{}(static_cast<int>(id));
               }
           };
       }

---

3. KEEP the human-readable name inside `PalletSpec` for display purposes.

   Reason: string names are still needed for the UI (displaying the list to
   the user). They must not be the lookup key, but they belong as a field
   inside `PalletSpec`. Verify that `PalletSpec` in `Types.h` already has a
   `std::string name` field. If it does not, add one.

---

4. UPDATE the usage comment in the header to reflect the new API.

   Replace the example:
       const PalletSpec& spec = PalletRegistry::lookup("EPAL_EUR_1");

   With:
       const PalletSpec& spec = PalletRegistry::lookup(PalletID::EPAL_EUR_1);

   Also remove or update the key naming convention comment since string keys
   no longer exist. Replace it with a note that the enum naming convention
   follows the same REGION_OPERATOR_WxL pattern.

---

5. UPDATE every existing call site of `PalletRegistry::lookup` in the
   codebase to pass a `PalletID` enumerator instead of a string literal.
   Do not leave any string-based call sites in place.

---

Do not change any logic in the packing algorithm or any other behavior.
These are purely API and type-safety changes.
```