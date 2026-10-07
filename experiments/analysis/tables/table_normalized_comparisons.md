| Benchmark              | A->B Transfers     | A->B Allocs        | A->B Worklist    | B->C States      | B->C Worklist    | B->C Transfers   | B->C MDE State Reuse Rate   |
|:-----------------------|:-------------------|:-------------------|:-----------------|:-----------------|:-----------------|:-----------------|:----------------------------|
| bm01_simple_alloc      | 0.0% (unchanged)   | 0.0% (unchanged)   | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 66.7% (reuse)               |
| bm02_linked_list_build | 0.0% (unchanged)   | 0.0% (unchanged)   | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 66.7% (reuse)               |
| bm03_dead_pointers     | -50.0% (reduction) | -66.7% (reduction) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 66.7% (reuse)               |
| bm04_aliasing          | 0.0% (unchanged)   | 0.0% (unchanged)   | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 66.7% (reuse)               |
| bm05_branch_alloc      | 0.0% (unchanged)   | 0.0% (unchanged)   | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 66.7% (reuse)               |
| bm06_loop_list         | 0.0% (unchanged)   | 0.0% (unchanged)   | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 66.7% (reuse)               |
| bm07_multi_field_graph | -10.0% (reduction) | -25.0% (reduction) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 66.7% (reuse)               |
| bm08_genuine_loop      | 0.0% (unchanged)   | 0.0% (unchanged)   | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 0.0% (unchanged) | 81.2% (reuse)               |