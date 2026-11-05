let library;

export function initialize(data) {
  library = data;
}

export function resolve(specifier, context, nextResolve) {
  return nextResolve(specifier === library.url ? library.path : specifier, context);
}
