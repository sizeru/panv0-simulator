# On managing the spm stack
- The SPM stack grows when attempting to add a new value to it
- When a new SPM value is pushed, it learns the size of its content by the
  operator 'size'. A size of 0 indicates a pointer in this case.
- The SPM stack should shrink when the anchor is reduced

# On the general project
- What we're trying to avoid is "This could run on this architecture if we
  recompiled it." Yes building from source is best, but in practice this is not
  how it happens. The goal is to get back to "copy a program from one computer
  to another", with highly diverse architecture.
- We should talk about the advantage of using a composition stack over a
  traditional eval stack. It is because we may choose where data goes. In truth,
  there's no telling whether this is better or worse than a traditional eval
  stack, and future work should create a compiler to test these differences.
  It's likely possible to lean into the stack-based idea and remove SPM
  registers entirely.
