
* almost like rubber duck programming with a rubber duck that actually responds
* no vibe coding, avoided code generation because i don't like to review code, instead as an 
  oracle that could look up information, condense specs, help me debug stack traces, create
  reference implementation of algorithms adapted for my use (stackoverflow on steroids, basically)
* the way i do problem solving: i start by trying to write code, and realise that the architecture
  needs reworking while writing code, and doing refactoring on the fly; using agents is too much
  extreme waterfall approach, fine for small projects where one knows what they want, not so fine 
  when project involves learning
* i deliberately avoided reading mold/gold/lld source code to avoid copying the design
* used C because that's probably what it is best at
* linker, for personal reasons (to understand) but also ideal because well known problem
* only free models, because tools should be free
* extremely good at RAG, summarized blog posts by gold linker author, got information about mold, 
  knows ELF spec etc, writing code by spec is most likely dead


## Pitfalls
* a lot of wild goose chases because it never understands that the test code might not be correct
    * tsan debug flag, ndebug
    * printf %x vs %lx
    * `deque_push((void*) 0)`, `value = (uintptr_t) deque_pop`
* hallucinates constants beyond the most trivial examples
* very good at seemingly complex debugging, incorrect left vs right pointer, detected circular ownership when still refcounting
* very good at complex debugging part 2: understands memory leaks/stack traces/valgrind output, tsan output, profiling
* very good at regurgitating correct algorithms, a perfect rb tree implementation 
  for example, and even understands when to use them for well-known use cases, but
    very bad at determining skill level and overall purpose / context, always pushes
    for very advanced patterns when not needed
* certainly not phd-level, many cases it tried to make me optimize for the wrong thing because it is in its training data
  that robin hood hashing is the optimal hash algorithm for example, but ignores that this is bad for concurrency
* bad at code correctness, if correct code but deviates enough from training data set
* very good at adapting to your code style, but sometimes annoying that it doesn't correct 
  incorrect terms and continue to use your terminology
* annoying that it never outputs tthe same example twice, always variations, and 
  if you get an elegant solution once you will never get it again
* it _always_ has to comment on something, if you post mostly correct code
  and ask about the idea, it will instead nitpick about compilation errors 
* it has no idea what numbers/performance means and combined with extreme
  sycophancy it is "lethal"; it will claim that 500 microseconds for inserting 1M elements
  into a hash table is good performance in one instance and when you push back it will say that it is very bad,
  it has no accountability or understanding
* it feigns expertise a lot, it will for example insist that robin hood hashing is ideal
  and make excuses for why contention doesnt matter
* drowns me in flattery all the time
* llms are stochastic, natural language is imprecise; programming languages are formal,
  which not only makes it easier to reason about but also can expect a reliable and reproducable
  outcome
