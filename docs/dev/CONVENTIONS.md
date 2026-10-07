# Coding Conventions

### C++ Naming Conventions

- Interface names (classes containing only pure virtual methods with no implementation) start with `I`, as in C#.
- Class names and public method names use PascalCase.
- Private method and field names use camelCase.
- Function argument names use camelCase.
- Template parameter names follow the C# style: `TKey`, `TValue`, `TIterator`.

---

Comments in code can only be added to indicate areas of [technical debt](TechnicalDebt.md) (only by a human), the end of `#endif`, and the end of namespace.

[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/)

There's no need to add image files to the repository. You can add images to the [gist](https://gist.github.com/boykopovar/0e53f2e1426f29ecd41e3b51540b8a90) comments and paste a link into the md file.
