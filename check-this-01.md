This is the "Pre-Flight Check." I have reviewed every line of code we wrote and every decision we made.

There are **three specific "Invisible" mechanics** that we used in the examples but haven't explicitly defined in the architecture yet. If we don't define them, the compiler will break when you try to write that Merge Sort.

### 1. The Indexing Decision (0 vs 1)

In your Merge Sort example, you wrote:
`let i, j be 1`

This implies **1-based Indexing** (like Lua, Julia, or MATLAB), not **0-based** (like C, Java, Python).

* **0-based:** First item is `list[0]`.
* **1-based:** First item is `list[1]`.

**The Issue:**
In C (the implementation language), arrays are 0-based.

* **Decision:** If you want Moon to be 1-based (which fits the "English" vibe better), your VM instructions (`OP_GET_SUBSCRIPT`) must effectively do `array[index - 1]` internally.
* **Recommendation:** Stick to **1-based**. It feels more natural for a language that says `first item` rather than `zeroth item`.

### 2. The "Range" Literal (`to`)

We used this syntax heavily:
`array[1 to mid]`
`for i in 1 to 10`

**The Missing Piece:**
We need a **Range Value Type** in the VM.

* When the compiler sees `1 to 10`, it shouldn't generate a Loop immediately. It should create a lightweight struct: `ObjRange { start: 1, end: 10 }`.
* **Why?** Because `add` needs to know if it's adding a *Range* of numbers or just one number.
* **Implementation:** You need a `TOKEN_TO` and an `OP_RANGE` opcode that pops two numbers and pushes a Range object.

### 3. The "Slice" Operator

We casually wrote `array[1 to mid]`.
Standard arrays don't support ranges as keys.

**The Missing Logic:**
We need to overload the subscript operator `[]`.

* If key is **Number**: Return single item (`list[1]`).
* If key is **Range**: Return a **New List** containing the subset (`list[1 to 3]`).

---

### Final "invisible" specs

To make your reference file compile, we must lock these in:

| Feature | The Decision | Implementation Note |
| --- | --- | --- |
| **Comments** | **`@@`** (Double At) | Scanner ignores everything after `@@` until newline. |
| **Booleans** | **`true`, `false**` | `TOKEN_TRUE`, `TOKEN_FALSE`. |
| **Logic Ops** | **`and`, `or`, `not**` | Use English words, not `&&`, ` |
| **Equality** | **`is`**, **`is not`** | `if x is 5`. (Replaces `==`). |
| **Inequality** | **`<`, `>`, `<=`, `>=**` | Standard math symbols are fine. |
| **Null** | **`null`** | The "Empty" value. |
| **Modulo** | **`mod`** | `10 mod 3` (English style) vs `%`. |

### The Updated Project Plan (Phase 1.5)

I am adding a small sub-phase to handle these "Invisible" types.

**Phase 1.5: The Glue**

* **1.5.1:** Implement `ObjRange` (Start, End).
* **1.5.2:** Implement `OP_SLICE` (Extracting logic).
* **1.5.3:** Adjust all array access to be **1-based** (decrement index in C).

---

With these 3 points clarified, the specification is **complete**. You have a consistent world where "English" logic applies to syntax, math, and indexing.

**Are you ready to create the `main.c` file?**
