# Prompt for Implementing a Robust List Data Structure in ABC

## Context
I need to implement a fundamental list data structure for the ABC framework (https://github.com/berkeley-abc/abc). ABC is a system for sequential logic synthesis and formal verification developed at UC Berkeley. The implementation should maintain ABC's code quality standards and integrate seamlessly with existing components.

## Implementation Requirements
Please help me implement a robust, general-purpose list data structure in ABC with the following characteristics:

1. Follows ABC's coding conventions and memory management patterns
2. Provides standard list operations (create, destroy, add, remove, iterate)
3. Handles arbitrary data types through void pointers
4. Includes comprehensive error handling for all operations
5. Minimizes memory fragmentation and prevents memory leaks
6. Implements proper iterator mechanisms compatible with ABC's traversal patterns
7. Offers efficient access to both ends of the list
8. Includes thorough documentation in ABC's standard style

## Expected Functionality
- List creation and destruction
- Adding/removing elements from both ends
- Insertion/deletion at arbitrary positions
- List traversal and manipulation
- Memory-efficient implementation
- Thread-safety considerations
- Proper error state propagation

## Robustness Focus
The implementation should be particularly robust against:
- Out-of-memory conditions
- Invalid inputs
- Concurrent access issues
- Edge cases (empty lists, single-element lists)
- API misuse (e.g., accessing after free)

## Example Code
provide example implementations of vecPtr.h,
please refer to the attach code. 



The code should be production-ready and match the quality standards found in the existing ABC codebase.