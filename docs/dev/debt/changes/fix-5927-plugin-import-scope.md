# relinker

## add Functional

- [Guest-module import scopes](../../../core/relinker/relinker/src/guest/GuestModuleBuilder.cpp) do not model an undeclared module added to a process-global symbol namespace at runtime. Plain named plugin imports use the module, its declared guest dependency closure and the existing global scope of startup/bundled modules before host libraries; unrelated runtime plugin directories are excluded. Qualified console NIDs retain legacy global resolution when module metadata is absent. macOS still refuses duplicate guest-export names when an unqualified import has one scoped guest provider; provider-specific macOS binding is not implemented. Console interposition behavior and RetroArch core execution remain unverified.
