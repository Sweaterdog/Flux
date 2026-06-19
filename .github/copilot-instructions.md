# Rules for developing Flux

1. You MUST stay within C++, you *can* use C if absolutely necessary, but you cannot write anything, even if temporary, in Python or C++.

2. All standard libraries for Flux MUST be written in C++, this makes it so it is very easy for Flux to use low level hooks, and APIs such as CUDA, OpenCL, and Vulkan.

3. If you don't have it in context already, read `Flux_Language_Manual.md` to understand the language design.

4. When making changes to Flux, or adding new features, you MUST update the documentation in `Flux_Language_Manual.md` to reflect the new items in a new catagory, or expand on the catagory. DO NOT include things such as "I added this feature," or "this new feature," no, it just needs to be plain, Markdown Documentation.

5. The code should be well commented, in English, and be extremely readable / easy to trace back to. Avoid clumping tons of code together into single files, use folders and seperate folders if necessary. The code *shouldn't* be organized in such a way where a function has it's own file. Group multiple helper functions together inside of a single file if they operate very similarly, or may require eachother to operate.

6. The goals listed inside of the Flux Language Manual are the goals for the language, keep it to the book. If you're every unsure on how to tackle something, ask the user about it. Clarify what you *think* it says. Update `copilot-instructions.md` if you find something else new, add it in the `items to remember` section below

7. When adding a feature, don't move on until you have tested it. The folder `test_suite` should contain folders for each feature, run extensive tests, and fix the code there if errors exist, or if the operation performs different than expected.

8. DO NOT flood the root directory with documentation files, the Flux_Language_Manual.md was made by humans. You *are* allowed to put `extra_docs` inside of the `.github/extended_docs/` folder, put anything you feel should be noted in there, and you don't need to worry about updating information if you'd rather take the much messier `ITEM_ID#1_UPDATED_FINAL_6.md` route, but try to keep it clean and organized.

9. You made yourself a document, called `best_practices.md` in the `.github/` folder, this is for you to follow. Establish your guidelines for writing code, and follow them. Ensure you stay to the book. If you don't have it in context, add it ASAP!

10. Don't stub code. You know that if you stub it, you will have to work on it later. Do not procrastinate, officiate.

11. When finished, instead of ending your response use the avaliable tools to ask for confirmation that you are allowed to stop. Ensure the user has ample room in order to provide feedback on what to continue working on.

12. Every 5 tool calls, check to see if the `./github/user_feedback.md` file has any new content. If it does, do what the file says

## Items to remember

1. **Copy-vs-reference trap:** When iterating `list<StructType>` and assigning `StructType var = list[i]`, this creates a VALUE COPY. Mutations to `var` are silently lost. Always use `list[i].field = value` directly for mutations. See best_practices.md "Struct-in-List Mutation Rules."

2. **Pointer dereference in unsafe blocks:** `*ptr = value` must be handled in BOTH `emitNode` (DEREF_ASSIGN case) AND `emitExpr` (expression context). The parser wraps these in `EXPRESSION_STMT`, so `emitExprStmt` calls `emitExpr`, which needs its own DEREF_ASSIGN handler.

3. **Freestanding heap:** The kernel's `flux_heap_alloc` uses a 256KB emergency buffer until the real heap is connected via `*heapPtrPtr = (byte*) heapChunk`. If the heap pointer assignment fails silently (like the old `/* [expr 20] */` bug), all FluxList allocations are limited to 256KB, which overflows during boot and causes data corruption.

4. **String concatenation:** The transpiler does NOT always generate `flux_str_concat` for `string + int` via the `+` operator. Use string interpolation `"${var}"` instead for reliable concatenation.

5. **Alt key handler structure:** All `if (event.alt)` checks must be in a SINGLE block with elif chains, not separate if-blocks after a blanket `continue`. Otherwise, later checks become dead code.