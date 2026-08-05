MOON: A DYNAMICALLY TYPED PROGRAMMING LANGUAGE LEVERAGING A NATURAL-LANGUAGE INCLINED SYNTAX TO IMPROVE ALGORITHMIC INTUITION 

### By 

### UKPAI, MUNACHISO HENRY 190805023 

A PROJECT SUBMITTED TO THE SCHOOL OF UNDERGRADUATE STUDIES IN PARTIAL FULFILLMENT OF THE REQUIREMENT FOR THE AWARD OF BACHELOR OF SCIENCE (B.Sc.) IN COMPUTER SCIENCES, UNIVERSITY OF LAGOS 

MARCH, 2026 

# ABSTRACT 

Natural-language pseudocode is an important component of teaching fundamental algorithmic logic in computer science education. But when these conceptual models are converted to software, a considerable cognitive burden arises. The inflexible nature of traditional industrial language syntax, with its machine-oriented terminology and structure, often confuses novice programmers, shifting the pedagogical emphasis of the subject towards memorizing grammar rules and syntax rather than problem-solving. To overcome this obstacle, this project proposes designing and implementing MOON: a dynamically typed, text-based programming language that reads and compiles natural language pseudocode directly. The system architecture includes a fully written compiler frontend and a high-performance, stack-based virtual machine backend. Using a Top-Down Operator Precedence (Pratt) parser, the language can solve complex, dynamic, infix operations and natural language syntax without having to worry about the structural restriction of the traditional recursive descent parsing. Additionally, the implementation engine automatically manages dynamic memory allocation and polymorphic functions to ensure smooth computational throughput. The project aims to show that a language can reduce syntactic boilerplate to support algorithmic intuition while retaining the architectural strength of low-level system execution. 

ii 

### TABLE OF CONTENTS 

|Abstract................................................................................................................................................ ii|
|---|
|Table of Contents................................................................................................................................iii|
|**CHAPTER ONE: INTRODUCTION**..............................................................................................1|
|1.1<br>Background of the Study......................................................................................................1|
|1.2<br>Statement of the Problem.....................................................................................................1|
|1.3<br>Aim and Objectives..............................................................................................................2|
|1.3.1<br>Aim of the Study..........................................................................................................2|
|1.3.2<br>Objectives of the Study................................................................................................2|
|1.4 Significance of the Study...........................................................................................................3|
|1.5 Scope and Limitations of the Study...........................................................................................4|
|1.5.1 Scope of the Study...............................................................................................................4|
|1.5.2 Limitations of the Study......................................................................................................4|
|1.6 Organization of the Project.........................................................................................................5|
|1.7 Definition of Terms....................................................................................................................5|
|**CHAPTER TWO: LITERATURE REVIEW**..................................................................................7|
|2.1 Introduction................................................................................................................................ 7|
|2.2 The Evolution of Programming Paradigms and Syntax.............................................................7|
|2.2.1 From Machine Code to High-Level Abstractions...............................................................7|
|2.2.2 The Cognitive Load of Syntax............................................................................................8|
|2.2.3 Pseudocode as a Pedagogical Tool......................................................................................8|
|2.3 Theoretical Foundations of Language Implementation..............................................................8|
|2.3.1 Compilation vs. Interpretation Models................................................................................9|
|2.3.2 The Architecture of Process Virtual Machines....................................................................9|
|2.3.3 Lexical Analysis and Tokenization......................................................................................9|
|2.4 Advanced Parsing Methodologies............................................................................................10|
|2.4.1 Recursive Descent Parsing and Its Limitations.................................................................10|
|2.4.2 Top-Down Operator Precedence.......................................................................................10|
|2.5 Memory Management in Virtual Machines..............................................................................11|
|2.5.1 Dynamic Memory Allocation............................................................................................11|
|2.5.2 Internal Object Representation: Arrays and Hash Maps...................................................11|
|2.6 Review of Existing Educational and Scripting Languages......................................................12|
|2.6.1 Python: Readability vs. Architectural Overhead...............................................................12|
|2.6.2 Lua: Lightweight Embedding and Procedural Rigidity....................................................12|
|2.6.3 Visual Block Languages....................................................................................................13|
|2.7 Summary of Literature and the Identified Gap........................................................................13|



iii 

|**CHAPTER THREE: METHODOLOGY**......................................................................................14|
|---|
|3.1 Introduction.............................................................................................................................. 14|
|3.2 Choice of Implementation Environment..................................................................................14|
|3.3 High-Level System Architecture..............................................................................................15|
|3.4 Language Design and Formal Grammar..................................................................................17|
|3.4.1 The Design Philosophy of Natural-Language Syntax.......................................................17|
|3.4.2 Lexical Specification and Primitives.................................................................................17|
|3.4 3 Syntactic Formalization (Context-Free Grammar)............................................................17|
|3.5 Phase 1: Lexical Analysis (The Scanner).................................................................................19|
|3.5.1 The Sliding Window Architecture and Zero-Copy Tokens...............................................19|
|3.5.2 Keyword Resolution (The Trie Data Structure)................................................................19|
|3.5.3 Context-Sensitive Lexing for String Interpolation............................................................20|
|3.6 Phase 2: Syntax Analysis (The Pratt Parser)............................................................................21|
|3.6.1 The Abstract Syntax Tree (AST) Data Model...................................................................21|
|3.6.2 Implementing Top-Down Operator Precedence (The Core Engine).................................22|
|3.6.3 Advanced Syntactic Desugaring and AST Inversion.........................................................24|
|3.6.4 Contextual Operator Chaining ("Sticky Subjects")...........................................................25|
|3.6.5 Phrasal Function Resolution (The Signature Automaton).................................................26|
|3.7 Phase 3: The Compiler Backend (Bytecode Generation).........................................................27|
|3.7.1 The Instruction Set Architecture (ISA)..............................................................................28|
|3.7.2 Bytecode Chunking and the Constant Pool.......................................................................29|
|3.7.3 Scope and Symbol Resolution...........................................................................................30|
|3.8 Phase 4: The Virtual Machine Architecture..............................................................................31|
|3.8.1 Dynamic Value Representation (Polymorphism and Memory Models)............................31|
|3.8.2 Execution State and Scope Isolation (Activation Records)...............................................32|
|3.8.3 The Fetch-Decode-Execute Cycle and Polymorphic Dispatch.........................................33|
|3.9 Architectural Design of Core Data Structures..........................................................................35|
|3.9.1 Dynamic Arrays (Lists).....................................................................................................35|
|3.9.2 Hash Maps (Dictionaries and System Registries).............................................................36|
|3.10 The Native Method Interface (The Bridge)............................................................................38|
|3.11 Memory Management and Garbage Collection.....................................................................39|
|3.11.1 The Allocation Problem and Dynamic Lifespans............................................................39|
|3.11.2 Object Tracking and the Unified Memory Graph............................................................40|
|3.11.3 The Mark-and-Sweep Algorithm.....................................................................................40|
|3.12 System Diagnostics and Fault Tolerance................................................................................42|
|3.12.1 Compile-Time Resilience (Structural Synchronization).................................................42|
|3.12.2 Dynamic Fault Interception (Execution Graph Unwinding)...........................................43|
|3.13 Conclusion..............................................................................................................................44|
|**REFERENCES**................................................................................................................................. 45|



iv 

### **CHAPTER ONE: INTRODUCTION** 

#### **1.1 Background of the Study** 

Computer science history is actually a history of abstraction. In the early days of computerization, the programmers needed to write machine specific instructions. It was a boring task and it was directly correlated to the sophistication of hardware, physical addresses and memory registers. Over time, the industry came to realize that developers were not to be tied to the hardware so that a bigger and scalable system would be built. This was taken to the actualization where high level languages such as C and object-oriented languages such as Java came in. These languages had a capability of abstracting the hardware states and hence making the programmers focus on the software architecture and algorithm writing. And as the execution environments became stronger and more full-fledged, the grammatical regulations involved in using these surroundings became rigorous. Today, commercial programming languages are enormously powerful, but they have a syntactic overhead they impose, which makes them costly in the sense that it is paid initially, by programmers, before they can get a computer to perform even the most basic of tasks (Scott, 2015). 

Even after decades of evolution in the sphere of language design, there has remained an extremely noticeable lack of connection between how a programmer would approach a problem and how a computer expects an answer to be given. This alienation is more pronounced and especially in places where there is learning. Pseudocode is used nearly universally when the basics of computing are being taught in a classroom. It is the case that pseudocode is ideal because it allows the person to devise algorithmic solutions using natural language logic, without being limited to any machinereadable grammar. 

The slight problem occurs when such students transfer to the computer lab. When novice programmers engage in the task of making the effort to translate their mental models into actual code, they are brought to a halt. They are forced to devote natural language thoughts to rigid, nonintuitive syntax structures. The mental load then suddenly no longer assumes that it needs to comprehend the underlying technology of the algorithm but simply recalls remembering to use statement terminators and to appease the fussy type checkers. This creates great a degree of psychological stress. In the empirical studies done in the domain of teaching computer science, it has been noted that in most instances, traditional C-style syntax is rather a hindrance rather than a support. In fact, when a study experimenting the placebo effect in a programming syntax found that beginners who use a less symbolic and more traditional syntax do not achieve the desired accuracy as well as beginners using a language based on randomly generated keywords (Stefik and Siebert, 2013). It is a tremendous barrier to the process of problem-solving thought to code because of this syntax barrier. It is one of the biggest causes of frustration and turnover rates that can always be observed in introductory computer science courses of novices. 

#### **1.2 Statement of the Problem** 

Almost all of computer science education is based on the principle of algorithms thinking. Only after a beginner programmer is taught how to resolve a problem in the whiteboard using a pseudocode is he or she then sat at a keyboard to even compile a program. The approach is very effective as the pseudocode will have allowed following the logical steps sequentially in natural language without having to be restricted by the rigid rules of machine grammar. This crisis comes right at the point when such conceptual models should be transferred into programs that can be implemented into the practice. The focus of all the exercise is abruptly altered. Student A is not optimizing the efficiency or testing edge cases of the algorithm, but instead is devoting disproportionate periods of time to debugging missing semicolons, mismatched brackets, and mysterious compiler errors (Luxton-Reilly et al., 2018). The root of the issue here is that the present condition of learning to program entails that one must learn a fixed, machine-focused 

1 

communication protocol prior to the final user being empowered to communicate in any manner to allow one to make any computational thought. 

The existing industrial programming languages contribute to this issue directly, by introducing what is informally known as accidental complexity due to software engineer Frederick Brooks (Brooks, 1987). Such languages as Java, C++ and C# were developed by these developers to develop largescale enterprise architecture. Their original purpose was not to explain to an amateur the methodology of a simple sorting algorithm. Nevertheless, to compose a simple print statement in these settings, a student will have to memorize class definitions, inflexible method signatures and memory allocation policies. They are lower-level principles of architecture, and have nothing to do with the immediate logical problem which they are trying to deal with. Even explicitly written dynamically typed programming languages, such as Python, have cognitive taxes associated with them. Python has strict, implicit restrictions on indentation rules of whitespace, which are not manifest and are the result of special keyword settings which are not naturally translated into everyday speech. The cost of simply deploying the implementation environment is so huge in practically all currently existing paradigms, compared to the logic under test. 

To avoid this syntax barrier, some educational curricula have been changed to block-based visual languages. Even though such graphical systems will never commit syntax errors, they will create an illusion of being safe, and will not expose one adequately to confront the reality of software engineering in text form. Once a visual sandbox is abandoned by a student, it will be horrifying to jump to the language of C-family. The openness in the software ecosystem to be bridged remains that of a text-based program environment capable of being read sequentially such as pseudocode but executed as a standard script. 

What follows because of the lack of language to fill this gap will not be a mere design challenge, but a problem, which requires some systems engineering. The compiler design required to compile grammar that has to validate natural-language structures in a dynamic manner would be highly specialized. The standard ways of parsing, such as the standard recursive descent, are not always effective in recognizing the dynamic precedence and infix operations that enable human language to be read. Therefore, the fundamental problem that the project is expected to address is two-fold. First, it has to address the pedagogical imperative of having a language that seeks to reduce boilerplate that is syntax based in nature. Second, it will have to design a high-performance, durable C-based compiler pipeline, which is able to scan, parse, compile, and interpret the said syntax without the end user ever seeing the underlying architectural complexity. 

#### **1.3 Aim and Objectives** 

##### **1.3.1 Aim of the Study** 

The main objectives of this project are to design, architect and code a dynamically typed programming language known as MOON, with a custom-built Virtual Machine. It is hoped to show that it is possible to have a natural-language syntax with little performance and architectural structure required in low-level systems execution can bridge the gap between the language and novices. 

##### **1.3.2 Objectives of the Study** 

To achieve this objective, the design and engineering process was broken down to five goals that were quantifiable. Specifically, the purpose of the project is: 

1. To define language lexical and syntactic grammar of the MOON programming language. This involves developing a syntax that is simple to read like pseudo code, but unambiguous, so that it can be computed by a machine. 

2 

2. To compose a lexical analyzer (scanner). This component will be intended to scan raw strings of source code to efficiently to convert them into a sequence of valid tokens, and edge cases like dynamic string literals and identifiers will be dealt with correctly. 

3. To write a Top-Down Operator Precedence (Pratt) parser. Even typical recursive descent parsers may be prone to unravel with poor grammar. With a Pratt parser, any infix operation and operator precedence level is dynamically analyzed and an Abstract Syntax Tree (AST) is constructed of the tokenized input. 

4. To design a compiler. The code generator will be a frontend-backend interface between the frontend and the backend, going through the AST to transform conceptual high-level nodes into a flattened, optimized modified set of instructions, expressed as in form of a bytecode. 

5. To develop a stack-based Virtual Machine in C. This is the primary execution motor. It aims to build in the internal dispatch loop, dynamically assign memory to the runtime environment, and offering native data structures to support dynamic scripting. 

#### **1.4 Significance of the Study** 

Software engineering is now torn between two poles on the issue of education. The visual blockbased languages offer the advantage of an error-free and safe environment on the one hand but fail to expose the student to the text-based development on the other hand. On the other end, industrial languages demand learning of complex, machine-oriented syntax, before one student is able to execute a simple algorithmic concept. The key significance of MOON programming language lies in the fact that it may assist in filling this pedagogical gap between these two extremes. MOON does not require a student to memorize syntax because the syntax under the syntax reflection syntactically mirrors the pseudocode that students learn in classroom (Robins, Rountree, and Rountree, 2003). This will allow computer science curricula and self-learners to devote more brain power to the basics of computational logic, control flow and data manipulation. MOON can be used to reduce to a minimum the frustration that can be encountered in the early-stages that tends to put many people off in introductory programming modules. 

But the practicality of such a study is much connected with introductory education. Using the strict meaning of systems engineering, the architecture of construction of MOON is a successful one to create a modern-day general-purpose scripting language. This project (linking a frontend assisted by a highly expressive, natural-language interface with a very low-level, C-backed Virtual Machine) demonstrates that very high readability does not necessarily conflict with the complete trade-off of execution speed. The MOON internal architecture is an implementable, documented roadmap to integrating a Top-Down Operator Precedence (Pratt parsing) implementation and a dynamic stackbased execution pipeline. It has made an easy to replicate methodology available to the academic and research community to design the lightweight interpreters and no longer rely on large, existing compiler frameworks like the JVM or the LLVM (Aho, Lam, Sethi, and Ullman, 2006). 

Practically, MOON provides instant functionality, as a fast scripting and prototyping language, which is accessible to programmers of any caliber. MOON can be written and executed on the spot when the engineer requires to test raw code of algorithms, e.g. a sorting algorithm very quickly. This completely eliminates the tedious setup of the environment, typing and waiting period necessitated by some programming language like Java or C++. 

Finally, the sheer technical size of the project of creating this end-to-end pipeline is an exercise in computer science basics. The MOON compiler construction consisted of walking the memory management stack, implementing data structure dynamic memory allocators, and coded raw loop dispatching of the binary code. By succeeding in bringing together the most abstract human thinking to the world of low-level memory execution, this project has brought undergraduate software architecture to a new level, as well as has demonstrated that advanced compiler design is a topic that can be written in very clean and efficient way. 

3 

#### **1.5 Scope and Limitations of the Study** 

In order to manage architectural focus and to deliver the system within the time constraints, within an academic time frame, one must have a clear definition of the engineering boundaries of such a project. Defining what MOON language is, we preserve the integrity of the project, in addition to providing a definite criterion according to which to evaluate the final Virtual Machine. 

##### **1.5.1 Scope of the Study** 

The paper is devoted to end-to-end design, architecture and implementation of MOON programming language. Both the compilation front end and the execution back-end are developed in pure C. The installed system is: 

- Front-end Lexical and Syntax Analysis: Our frontend lexical analyzer (scanner) was a hand written lexical analyzer, accepting raw source code strings and transforming them into a discrete sequence of valid tokens. These tokens are simply fed into a Top-Down Operator Precedence (Pratt) parser. Our Pratt implementation is a technically a compiled code of a more complex recursive descent compiler that knows about complex operator precedence and generates an Abstract Syntax Tree (AST), which can interpret the pseudocode-like syntax of MOON. 

- Bytecode Compilation: The code generator of this group is self-contained, and it provides the interface between the human readable syntax and the machine. It moves up and down the AST tree, and collapses the tree to a linear and highly optimized sequence of bytecode instructions. 

- The Virtual Machine Execution Engine: Our interpreter is a stack-based and bytecode interpreter. This VM takes charge of its instruction dispatch loop, call frames and execution stack without any reference to the host operating system. 

- Data Structures and Memory Management: The language contains an in-built representation of a simple set of data types (numbers, Booleans, strings). Even better, it is additionally applied to dynamically managing objects. Our in-house additions of simple objects-oriented data types include dynamic array types (Lists) and hash map types (Dictionaries) that are addressable on their properties. 

- Native Interoperability: We have developed an inbuilt Native Method interface. This enables the Virtual Machine to utilize raw C functions as MOON objects, and any dynamic property may be evaluated. 

##### **1.5.2 Limitations of the Study** 

Due to the fact that MOON was specially designed as an educational bridge and a lightweight local scripting environment, extensive enterprise-level system design features were omitted. Strictly speaking, the architectural constraints of this project can be described to follow: 

- Execution Paradigm: MOON is purely a bytecode interpreted language. We do not compile to native operating system code (such as standard C or C++), nor do we use Just-in-Time (JIT) compiler to compile a bytecode into native machine code on demand. 

- Concurrency Model: Virtual machine runs in a single thread environment only. Concurrency advanced features like asynchronous event loops, multithreading and mutex locks are not within the scope of this implementation at all. 

- Network and I/O restrictions: The language is confined to local computation and algorithm execution. It has no native socket programming, routing of HTTP, and network requests. 

- Standard Library Footprint: MOON does not include a full standard library although the capability of having native C-bindings on the architecture is present. There are no modules of higher complexity file system manipulation, graphical user interfaces (GUIs), or advanced cryptography. 

- Memory Optimization: The Virtual Machine manages the dynamic object memory safely and tracks the memory to eliminate the simple memory leaks during script execution. 

4 

Nonetheless, adoption of more sophisticated, industry- standard memory management techniques, including generational, concurrent, or compacting garbage collection algorithms, was considered to be more than was required of an educational interpreter. 

#### **1.6 Organization of the Project** 

This study report about MOON programming language is subdivided into five sections. 

Chapter 1 is the background of the study, which defines the cognitive friction encountered by novice programmers, and the difficulty encountered in translating natural thought into code. It indicates the research aims, objectives, significance and scope of the project. 

Chapter 2 is a review of related literature covering the development of programming languages, the comparison of Ahead-of-Time compiler and Virtual Machines, and the discussion of how parsing is performed. 

Chapter 3 explains the methodology and system architecture, the implementation of the lexical scanner, Pratt parser, the generation of the bytecode and the VM stack functions. 

Chapter 4 talks about the topics of system implementation, testing, and evaluation; it shows that MOON is capable of performing complex algorithms and handling memory. 

Chapter 5 concludes the project, summarizing it, and making suggestions on how it can be improved. 

#### **1.7 Definition of Terms** 

To be concise and in order to give a strict technical structure to the concepts that will be addressed in this work, the following base terminologies will be defined in ways that they are applied to the architecture of MOON programming language: 

Abstract Syntax Tree (AST) A graphical representation of syntax of a source code. Unlike a concrete parse tree, an AST contains no redundant information about the grammar (parentheses to indicate implicit grouping, statements ending with semicolons, etc.), and reflects the organizational and logical operations of the code in a very strict way, before the generation of the bytecode (Aho, Lam, Sethi, and Ullman, 2006). 

Accidental Complexity A concept of software engineer Frederick Brooks, to define the issues programmers face that are not directly or necessarily in relation to the problem they are solving, but the inefficiency of their software development tools, rigid syntax in the programming language, or onerous environmental settings (Brooks, 1987). 

Activation Record (Call Frame) A temporary data structure which is localized to a subroutine and which is dynamically generated by the Virtual Machine when a subroutine is called. It imposes a strict geometry around the execution stack, that is, effectually isolates the global memory space, such that the active function has a local scope of zero-based local variables, and conceptually isolates the active function, and does not allow cross-scope memory corruption. 

Ahead-Of-Time (AOT) Compilation Compiling a high-level source code all the way into a native machine code executable that is specific to a target operating system and architecture without actually executing the program. This is in direct contrast to an interpreted language as is the case with MOON or a byte compiled language. 

5 

Amortized Time Complexity Mathematical method of studying the cost of structural algorithms to run in a sequence of operations. It is demonstrated, with the help of the dynamic arrays and hash maps of the Virtual Machine, that, with the growth of the Virtual Machine, occasional memory reallocations have a heavy linear cost of O(N), although this cost is greatly dwarfed by the numerous instantaneous inserts, making the operation cost, on average, constant, or of O(1). 

Bytecode This is a very small, low-level, intermediate code format of a compiled source code. The idea of bytecode instruction sets is that they have optimized and linear execution in a software interpreter (a Virtual Machine) rather than in an actual hardware processor. 

Context-Free Grammar Context-Free grammar A strict representation of syntactic structures of a programming language. It defines the potential combinations of how terminal tokens and nonterminal symbols can be recursively re-replaced and rearranged into valid structural expressions (such as statement of a variable or a loop) which are independent, in any way, of the remainder of the linguistic context. 

Deterministic Finite Automaton (DFA) A hypothetical state-machine definition of computation which is widely used in the lexical and syntactic phases of the compiler. It compares sequences of input, e.g. sequences of characters, by swapping among a finite set of geometric states, which are defined. 

Dynamic Typing A typing and memory system where variables are not compiled in a specific type (i.e. integer, string, etc.), but as a variable. Instead, the Virtual Machine locates and writes data types dynamically based on the value that is residing in the address of the memory. 

Garbage Collection A collection system of memory reclamation. Specifically, MOON uses a Markand-Sweep tracing collector, which blocks execution periodically to execute a recursive search over the active program graph (the Mark phase) and to then linear searches the global memory pool to transfer to the host operating system a safe amount of unreferenced, and therefore, dead objects (the Sweep phase). 

Lexical Analysis (Scanner) This is the initial step of compiler pipeline. An analyzer is a lexical scanner that reads the raw uninterrupted sequence of characters provided in a source file and breaks them down into a sequence of discrete meaningful linguistic units known as tokens (e.g., keywords, identifiers, literals) (Scott, 2015). 

Native Method Interface This is an architectural interface on the Virtual machine which allows high-level interpreted code scripts to make direct calls and execute low-level functions on the host programming language (in this case C). 

Top-Down Operator Precedence (Pratt Parser) This is a high-level parsing method that was initially outlined by Vaughan Pratt (Pratt, 1973). In place of a fixed set of recursive rules of grammar, a Pratt parser operates on the types of tokens as the objects on which the parsing operations and precedence are attached. This allows the compiler to derive the complex prefix, infix expressions and natural language constructions at run-time, without compromising the system architecture. 

Virtual machine (Process VM) A software element of the systems software that imitates an isolated execution environment. Unlike a system virtual machine, which goes through and executes the entire operating system with software, a process VM (as used in MOON) carries its own softwareimplemented execution loop, software-implemented instruction pointer, and software-implemented memory stack whose only purpose is to run compiled-byte-code with platform independence. 

6 

### **CHAPTER TWO: LITERATURE REVIEW** 

#### **2.1 Introduction** 

Implementation and design of a new programming language is not a lone endeavor. It has a long history in computer science, being a direct evolution of decades of work in both fields of educational psychology and the low-level architecture of systems. To offer the academic reasoning on whether or not the MOON programming language is valid as well as its inner architecture, the literature which defines how human beings write programming code and how the machine interprets the code will have to be considered. This chapter provides a detailed review of that literature arranged in a manner that it traces a line down the hierarchy of pedagogical concerns in computer science education down to the hard-core reality of compiler construction. 

First of all, the chapter explores the historical context of the programming paradigms, and the history of the industry. Our approach involves the historical sequence of the development of the industry in terms of raw machine code to the contemporary high-level abstractions, and it is achieved through a targeted study of the academic literature that quantifies the cognitive load imposed on novice programmers because of the rigid syntax. On this historical background, however, the review enters into the theoretical background of language implementation. We apply the architectural division between AOT (Ahead-of-Time) compilers and decoders of data in binary form, we consider the protocols of process virtual machines and we guess about the foundations of lexical analysis. 

The review then focuses on the algorithms approaches required to decompose natural-language grammar. We contrast the traditional approaches to parsing with Top-Down Operator Precedence (Pratt parsing) to find out the optimal theoretical approach to apply in the frontend of MOON. We also review the literature on virtual machine memory management on the issues of dynamic data structures, including hash maps and arrays, being implemented in a C-based runtime system. Finally, we critically assess the current educational and scripting systems such as Python, Lua, and visual block systems and the specific trade-offs in design. It is in the combination of all these diverse areas of research that the specific pedagogical and technical loophole in the software ecosystem is uncovered in this chapter necessitating the creation of the MOON programming language. 

#### **2.2 The Evolution of Programming Paradigms and Syntax** 

In order to know why a minimal-expressive language such as MOON is needed, it is important to initially look at the development of programming syntax and the psychological effects that the development has had on the beginner programmer. 

##### **2.2.1 From Machine Code to High-Level Abstractions** 

The beginning of software engineering was marked by the one-to-one correspondence between human instructions and hardware implementation. Programmers manipulated physical registers by direct manipulation of raw machine code and assembly language. This was found to be a deadly bottleneck in computational demands that had increased exponentially in the 20th century. The development of high-level languages of the first generation, i.e. FORTRAN and C, altered the paradigm of the way of computing to the core (Sebesta, 2012). These languages led to structured programming, in which programmers could specify logic as control structures (loops and conditionals) rather than quite literally as raw memory jumps. 

This industry shifted on to more object-oriented programming languages which include Java and C++ which could potentially be used in enterprise applications that are highly scalable and able to reuse code. There was an obscure cost, however, to this historical search following the abstraction. The language designers added more and more elaborate grammatical rules to be able to completely 

7 

protect the hardware against the programmer. The abstraction was capable of building bigger systems and syntax evolved into a highly rigid machine-based communication protocol, and that was a distant departure of the natural thought process of human beings. 

##### **2.2.2 The Cognitive Load of Syntax** 

It is a severe inflexibility of grammar that is not easy to teach. Whenever an individual is introduced to a formal industrial language, he/she is instantly confronted with what software engineer Frederick Brooks termed as the accidental complexity, the difficulties found within the tools, as contrasted to the computational problem (Brooks, 1987). To be able to print plain text in Java for example, the student should remember class declarations, access modifiers, and signatures of static void methods before he/she is able to put at least a line of logic on paper. 

This complexity is best explained in Cognitive Load Theory with regards to the psychological impact. The educational psychologist, John Sweller formulated a theory that the working memory of human beings possesses very minimal capabilities of processing new information (Sweller, 1988). When the learning power of a novice is slowed down by the harsh learning of syntax, e.g. semicolons, brackets matched, abstract class and type declarations, etc. then they are virtually left with no room to perform the actual algorithmic logic that they are attempting to learn. 

The level of this problem was measured empirically by Stefik and Siebert. They carried out their famous experiment on the context of writing programs in the syntax of a placebo language: they discovered that novice programmers who wrote programs in traditional C-compiled languages fared no better than novice programmers who wrote programs in a language whose few keywords were chosen randomly. It is shown in the literature in particular that traditional syntax constitutes a colossal artificial barrier of entry, to the extent that students are forced to grapple with the compiler rather than study computer science. (Stefik & Siebert, 2013) 

##### **2.2.3 Pseudocode as a Pedagogical Tool** 

Being conscious of this cognitive impediment, computer science teachers, worldwide, are applying pseudocode as the main didactical aid. A high level, informal description of an algorithm is the pseudocode. It utilizes the programming conventions of a standard programming language, but it is not intended to be run on a computer; instead, it is intended to be read by humans (Guzdial, 2015). With the elimination of the necessity to produce syntactically ideal code, the pseudocode allows students to apply their existing knowledge of conversational English to produce algorithms in a rational manner. 

It is a hypothetical deconstruction, which occurs in translation. However, as effective as the whiteboard presentation of logic in the form of pseudocode in universities is to inculcate the student with the knowledge of logic, when they must translate that natural-language logic into the strict nature of an executable C or JavaScript, they will experience frustration. Among the systematic reviews of the literature on introductions to programming, it can be observed that there is an evident lack of tools that perform direct execution of pseudocode (Luxton-Reilly et al., 2018). The gap between the articulation of an algorithmic thought in natural language and the execution of it on a machine is significant. It is this pedagogical friction that MOON programming language is designed to eliminate. 

#### **2.3 Theoretical Foundations of Language Implementation** 

The execution environment should be architecturally correct before a new syntax could be tested. The use of a programming language is essentially based on the translation of source text into a machine action. 

8 

##### **2.3.1 Compilation vs. Interpretation Models** 

Two major paradigms to be used in implementing the source code are Ahead-of-Time (AOT) compilation and Interpretation. A high-level source code with an AOT compiler, e.g., the GNU C Compiler (GCC), is directly translated into machine code specific to the hardware, and the program is not even executed (Cooper and Torczon, 2011). The derived executable is specific to a specific operating system and architecture (e.g. x86 or ARM). AOT compilation is fast-scorching, although expensive in terms of portability, and introduces a large compilation overhead to the development cycle. 

Conversely, pure interpreters run time interpretation of the source code. A tree-walk interpreter is one such interpreter, in which the code is divided into an Abstract Syntax Tree (AST), and logic is executed by recursively walking the tree. But pure interpretation is infamously slow with the baggage of re-examination of the syntax tree in loops. This paradox is solved in the modern dynamic languages, with a hybrid implementation: the bytecode virtual machine. The language contains a compiler frontend that does not compile the source code to the native machine code, but to a very small representation of a set of instructions called the bytecode. At the back-end is then a software-implemented CPU which is a direct interpreter of the bytecode. And this is the same architectural paradigm applied by MOON programming language that allows the language to possess a balance in platform independence, fast compilation and speedy runtime performance. 

##### **2.3.2 The Architecture of Process Virtual Machines** 

Process Virtual machines are software engines which execute compiled bytecode, and are traditionally classified into two architectural categories: Register-based architectures and Stackbased architectures. In register-based virtual machines, a collection of virtual registers is used to store data during execution (e.g. the Lua runtime or the Dalvik VM). The contents of these environments are explicitly defined instructions on which registers the operands are held. Stackbased virtual machines, including the Java Virtual Machine (JVM), CPython, and the MOON Virtual Machine use a Last-In-First-Out (LIFO) data structure on the other hand. In case an operation is performed, the operands are implicitly supposed to be on the top of stack; they are pushed off, calculated and the product is pushed back in. 

Modern compiler literature is strongly supportive of the use of MOON as a stack-based VM. Shi et al. (2008) developed an epic study, Virtual Machine Showdown: Stack Versus Registers, in which an enormous empirical comparison of the two architectures was carried out. Although the register VMs do take a fraction of the time in the instruction dispatch loop because they have fewer instructions, stack VMs have 2 gigantic engineering advantages. To begin with, the stack-basedbytecode is extremely small. The fact that instructions do not necessarily encode the address of a register means that stack bytecode on average is 26% smaller than register bytecode, resulting in enhanced cache locality (Shi, Casey, Ertl, and Gregg, 2008). Secondly, it is much easier to compile to a stack machine. The compiler backend does not require to execute the sophisticated graphcoloring algorithms to follow and assign virtual registers. As a stack-based OS, MOON makes its own bytecode generator lightweight, fast and very maintainable. 

##### **2.3.3 Lexical Analysis and Tokenization** 

Even though a language is AOT-compiled or is executed on a VM, the general flow of the translation process should and must always begin at Lexical Analysis. A computer merely sees the source code as a characterless array of characters. The compiler in itself is oblivious to the fact that the consecutive combination of the characters like l, e, t, signifying the declaration of a variable. 

The front-line component is commonly called the lexical analyzer and is the one that reads this raw stream of characters and groups them into meaningful units of language that are called lexemes (Nystrom, 2021). Once a lexeme has been identified the scanner will place the lexeme within a 

9 

discrete Token. The example will turn lexeme + into a TOKEN-PLUS and lexeme 100 into a TOKEN-NUMBER. 

In addition, there is a significant optimization mission of the scanner. It proactively identifies and eliminates characters that are not part of the computation machine such as whitespace, line breaks and human readable commentaries (Aho, Lam, Sethi, and Ullman, 2006). Lexical analysis phase is complete and at that point, the compiler has succeeded in eliminating all the unproductive textual clatter and what remains can be relayed on the parser to be structurally validated. 

#### **2.4 Advanced Parsing Methodologies** 

The compiler must then verify the structural grammar of the discrete tokens and assemble them into a hierarchical Abstract Syntax Tree (AST) after the raw source text stream has been lexically analyzed to generate a stream of discrete tokens. A choice of a parsing algorithm can not only define the speed with which a compiler will execute, but also the flexibility of syntax of the language. 

##### **2.4.1 Recursive Descent Parsing and Its Limitations** 

The most used hand-written parsers in modern software engineering are recursive Descent used with LL(1) grammars. In an example of a simple recursive descent implementation, non-terminals in the formal grammar of a language are directly mapped to a specific C subroutine (Aho, Lam, Sethi, and Ullman, 2006). This top-down design is enormously intuitive in the breakdown of inflexible block structures, such as an if statement or a while loop, because the execution stack flow is precisely the flow of the source code. 

But complex mathematical or natural-language infix expressions force its horrendous architectural issues upon recursive descent. As pure recursive descent must rely on the C call stack to provide the operator precedence, the compiler must introduce a new grammar rule, and thus a new recursive function, in which every single level of precedence of the language is mapped. To enable the usage of regular arithmetic, logic and bit-wise operators, the syntax of a single integer literal can instantiate over a dozen recursive functions before the value itself is finally used (Nystrom, 2021). This creates an unnecessary burden to the compilation pipeline. The left-associative operations (such as subtraction and division) are also inherent problems with recursive descent. The use of an unsophisticated left-associative rule, in its own right, would automatically induce an unlimited leftrecursion and therefore bring the parser to a stack-overflow, unless the grammar is refactored in a rather clumsy way through iterative EBNF loops (Bendersky, 2009). 

##### **2.4.2 Top-Down Operator Precedence** 

In order to avoid recursive bloat and left-recursion trap of conventional parsing, the MOON compiler frontend does not use pure recursive descent as a method of expressing things, but uses a Top-Down Operator Precedence algorithm instead. Pratt parsing was first presented by Vaughan Pratt at the very first Principles of Programming Languages (POPL) conference, and was a radical new approach to compiler design (Pratt, 1973). 

Since the algorithm developed by Pratt appeared to have lapsed into a kind of relative academic obscurity during the decades in which universities became obsessed with the theory of automata and table-driven generators of parsers such as Yacc and Bison. Nevertheless, the algorithm was experienced a huge industry revival when software architect Douglas Crockford used it to syntactically analyze JavaScript to use it in the JSLint static analysis' code, stating that it was easy to read, easy to write, highly efficient, and highly versatile (Crockford, 2007). 

A Pratt parser, in contrast to recursive descent, does not use semantic rules as associated with the overall grammar, but with parsing logic that is associated directly with the tokens. Within this 

10 

architecture, each token is given a numeric "binding power" (binding power) and a handle functions, as to whether it is used in a prefix context (e.g. the - in -5) or an infix context (e.g. the - in 10 - 5). 

An expression is not cascaded to a dozen functions on the way of the parser. Rather, it proceeds into a narrow loop of repetition, popping tokens, and constructing the AST, and stopping when it comes across an operator whose binding power is unambiguously inferior to the one it is currently running in. This is a perfect solution to the infix associativity problem. Pratt parsing can also analyze highly nested mathematics in a fraction of the time compared to the call stack by using a numeric binding threshold instead of the call stack. In the case of the MOON programming language that gives preference to natural-language structures often acting as dynamic infix operators, Pratt parsing offers the theoretical flexibility necessary to parse a pseudocode-like syntax without breaking down the underlying C architecture. 

#### **2.5 Memory Management in Virtual Machines** 

The memory management defines the performance of the language. Under a dynamically typed language like MOON, the Virtual Machine must completely hide the physical hardware memory layout in favor of the user, and do the data allocation, tracking, and organization dynamically. 

##### **2.5.1 Dynamic Memory Allocation** 

With low-level systems programming (e.g. standard C), the compiler manages or the user's system calls manage the memory. However, in an interpreted dynamically typed language, the size and length of variables are completely unknown, until the time they are executed. Therefore, the Virtual Machine must assume the role of a mediating memory manager, which requires blocks of heap memory on dynamically requested basis, by the host operating system. 

The literature relating to the dynamic storage allocation refers to the fact that invoking routines of the operating system memory (malloc and free in C) is in itself performance overhead. In a landmark survey of memory allocators, Wilson et al. (1995) discovered that an ineffective strategy of allocating memory will lead to excessive fragmentation of memory and intolerable execution latency. A potent Virtual Machine does not only require the memory to be on a memory-by-memory level. Instead, it employs geometric expansion algorithms, which store neighboring blocks of memory in huge blocks and uses them internal to it. This method minimizes expensive system invocation and enables the runtime environment to be very cache local and the data physically local in the L1/L2 cache of the hardware to be more readily accessed in a shorter time. 

##### **2.5.2 Internal Object Representation: Arrays and Hash Maps** 

The theoretical soundness of a given scripting language depends on the efficiency of underlying data structures of the language. MOON natively supports object-oriented paradigms in two significant structures, which are Lists (Dynamic Arrays) as well as Dictionaries (Hash Maps). According to literature, such structures must be able to provide a time complexity of the normal operations to make them viable to be implemented in the high-speed algorithm processes (Cormen, Leiserson, Rivest, and Stein, 2009). 

##### **_Dynamic Arrays (Lists)_** 

Dynamic arrays can be easily extended and the user can add new data as opposed to C where arrays have a fixed capacity. This is resolved in the literature of computer science through amortized doubling. Using a Virtual Machine at every address of a dynamically-sized array up to the current memory size, the Virtual Machine allocates a new, contiguous block of memory with a size which is a full order of magnitude larger than the current size, copies the already-existing elements into the new one, and releases the old one. This specific expansion operation is time-intensive, but mathematically, Cormen et al. (2009) demonstrate that as the number of expansions increases, the 

11 

frequency of the expansions grows exponentially slower with the size of the array which means that the amortized cost of adding the item in any of these expansions is strictly non-increasing (Cormen, Leiserson, Rivest, and Stein, 2009). This guarantee is an algorithmic guarantee that the lists of MOON are mathematically efficient like native low-level arrays. 

##### **_Hash Maps (Dictionaries)_** 

The Virtual Machine utilizes the Hash Maps in order to model objects with string-based properties. A hash map is a data structure where a hash algorithm is used to link a string key (e.g. name) to a numeric index, such that the item can be accessed directly in memory rather than having to search a list. Algorithms written by Donald Knuth about the topic of searching algorithms put much analytical importance on the mechanics of hash collision resolution (Knuth, 1998). The Virtual machine design requires that the collision between the string keys must be solved without any loss of data on mathematically hashing the same keys to the same index in the array. 

Linear probing (chaining) and open addressing are the most popular methods of dealing with such collisions in optimized Virtual Machines. Besides this, load factor of a hash map is used to determine how effective it is as a storage medium when comparing the number of entries that can be stored by the storage medium to the total size of the available memory. The Virtual Machine makes sure that the average-case time behavior of the dictionary property in question is optimum by adapting the size of the backing array of the hash map each time the load factor rips off a preset threshold (typically 75%). It is these same architectural theories that are being put to practice by MOON, so that its high-level, pseudocode-like syntax is pushed to mathematically optimum memory models. 

#### **2.6 Review of Existing Educational and Scripting Languages** 

To formulate the architectural and pedagogical statement of MOON, the existing programming languages that command the forefront in introductory computer science and the low-level scripting languages should be critically examined. 

##### **2.6.1 Python: Readability vs. Architectural Overhead** 

Python is regarded as the industry standard in an introductory programming due to its emphasis on readability of code and non-Java / non-C++ dense boilerplate (Koulouri, Lauria, and Macredie, 2014). The literature has pedagogical and architectural limitations though. Python has relatively clean syntax because it has demanding whitespace and indentation principles (PEP 8). Error in the case of invisible whitespace is the order of the day when using entry-level programmers, replacing the problem of semicolons error with the other variety of syntax frustration. Besides, Python is not a lightweight embedded system. The standard version of the CPython platform relies on an oversized standard library, an intricate global interpreter lock (GIL), and an oversized environment setup (it requires path settings and virtual environments). It requires way more than the spirit of minimal expressiveness to write the architecture that must support a simple script. 

##### **2.6.2 Lua: Lightweight Embedding and Procedural Rigidity** 

Lua is the standard of the industry regarding the lightweight, embedded, C-written virtual machines. Ierusalimschy, de Figueiredo, and Celes (2005) indicate in their original paper about the architecture of Lua 5.0 that Lua can be run with lightning speed just by a register-based virtual machine and optimum hash tables (Ierusalimschy, de Figueiredo, and Celes, 2005). MOON is a loosely based C-backed VM that is similar to Lua, i.e. being ultra-light. However, Lua was designed as an environment of professional software (specifically game engines) configuration and scripting. Consequently, it has very symbolic and procedural syntax (e.g., local x = 5). It is not aimed at reducing the cognitive-psychological gap between the algorithmic thinking of natural language, on the one hand, and the execution of a code, on the other. It is rather a helper to existing programmers, rather than an educator to beginners. 

12 

##### **2.6.3 Visual Block Languages** 

To eliminate syntactic friction in Python and Lua, visual block-based languages such as Scratch are frequently taught in schools (Resnick et al., 2009). These environments provide syntactically valid code by allowing the students to drag and drop logic blocks. Even though it has been relatively effective with children, there is a grim transition trap of university students according to academic literature. In a comprehensive research on the blocks-to-text transitions, Weintrop and Wilensky (2015) discovered that students who get to learn through block-based systems find it atrocious to get into text-based paradigms. Block languages reflect an abstraction of the realities of text manipulation, scope of variables and execution in sequence. In that sense, despite syntax errors being eliminated using visual environment, they are proactively slowing down the process of developing text-based software engineering skills that should be required. 

#### **2.7 Summary of Literature and the Identified Gap** 

The synthesized historical, psychological, and architectural literature demonstrates the apparent and measurable lack of the current software ecosystem. The cognitive barrier of traditional, highly symbolic syntax, which overwhelms the functioning memory and does not permit novices focus on algorithmic solution, has been shown in years of research on education (Stefik and Siebert, 2013; Sweller, 1988). Though any person may avoid this barrier on whiteboards by using pseudocode to step around it, an enormous friction point occurs when converting a pseudocode into an executable C or Java script. An analysis of the options that can be identified shows that the available solutions are partial. Python is vastly architecturally inefficient with inflexible, implicit whitespace constraints. Lua has the necessary lightweight C back end but has a strict and procedural syntax. 

Conversely, a visual language like Scratch entirely avoids text usage, and students are not prepared to operate in the environment of software engineering (Weintrop and Wilensky, 2015). Additionally, the theoretical literature of computer science has known that the natural-language and text-based structures cannot be effectively parsed using the traditional recursive descent parsers. It requires the advanced, dynamic capabilities of Top-Down Operator Precedence (Pratt, 1973) and the mathematical performance of a stack based virtual machine (Shi, Casey, Ertl, and Gregg, 2008) and amortized O(1) memory allocation models (Cormen, Leiserson, Rivest, and Stein, 2009). 

##### **_The Known Gap_** 

There is no known text-based, code-independently interpreted programming language written in natural-language pseudocode, that is, not compiled in C language. The necessity to have a minimally expressive language in the gap between visual blocks and enterprise code, is, unequivocally, justified in the literature. Therefore, the design and construction of the MOON programming language is not merely a bit of engineering, but scientifically and academically founded on a reaction to a so-called pedagogical crisis. 

13 

### **CHAPTER THREE: METHODOLOGY** 

#### **3.1 Introduction** 

Designing a programming language is at a special intersection between human cognitive psychology and crude computing power. And it is fundamentally not identical to the normal evolution of software application. It cannot be as straightforward as stitched-together algorithms and requires the entire translation of abstract and theoretical grammar into concrete and mathematically precise, engineering pipeline. The chapter gives a review of the underlying methodology, architecture, and precise theoretical design choices to use in architecting the MOON programming language. 

In an attempt to give this architectural blueprint its own legs in regard to the rigorous academic scrutiny, the system analysis as follows is presented with the express intent of directly tracing the map to the underlying goals as presented in Chapter 1. Contrary to displaying individual software modules, this chapter displays a thread of engineering that is in progress. The first one is achieved immediately in Section 3.4 because mathematical models of the lexical and syntactic rules of the language are presented. In later stages of the compiler pipeline, Phases 1 and 2 (Sections 3.5 and 3.6) execute Objectives 2 and 3 by giving an account of the conceptual architecture of the lexical scanner and the Top-Down Operator Precedence (Pratt) parser. In that case, Objective 4 (Section 3.7) can be fulfilled by explaining the process of creating a bytecode. The fifth goal is finally resolved in Phase 4, and the second analysis of the data structure (Sections 3.8 to 3.12), which is a complete representation of the stack-based Virtual machine, memory management protocols and system diagnostics. 

Last but not least, the chapter is chronological. It pursues one intellectual line of code. It begins with the design of a language form, which is processed by the frontend text-processing phases of the compiler which are then followed by a syntax-tree generating phase after which it gets to the bottom with the low-level execution and error-handling of the Virtual Machine. This methodology of process provides a clear, highly repeatable framework of this entire MOON software environment. 

#### **3.2 Choice of Implementation Environment** 

The host language choice is the cornerstone decision in any systems engineering project, particularly one as a complex as a compiler and a Virtual Machine. In the case of the MOON ecosystem, it was decided to design the complete pipeline using pure C, that is, to use the C99 standard and compile it using the GNU Compiler Collection (GCC). On the face of it, it would be very practical to write a modern educational environment using a structured, higher-level language (Java, C, Python, etc.) to quickly speed up development. But, in strict systems architecture terms, such a step causes a critical design failure often known as hidden overhead. A Virtual Machine is developed in order to behave like a software-modeled CPU; to behave so correctly, it needs a clean, very predictable environment of execution (Smith & Nair, 2005). 

Had the MOON Virtual Machine been written in a managed language such as Java, we would be overlaying one VM onto another. It would be fully left to the mercies of the Java Virtual Machine processes to execute MOON bytecode. Host language automated memory management systems, in particular, may cause random, "stop the world" (a phenomenon when execution is paused so unused memory may be reclaimed) latency spikes. Host garbage collectors stop all running threads of execution in order to free up memory just as extensively documented in research by Jones, Hosking and Moss (2011). MOON will not execute with the same degree of determinism should the host GC preempt our Virtual Machine during a cycle. Writing the system in C, we are explicitly denying this anti-pattern. We ensure that the Virtual machine has no hidden runtime tax, that it will execute exactly the machine code that we write, at the moment we write it. 

14 

In addition, basic to engineering a low-level interpreter is absolute, granular control of hardware and system RAM. Close arrays of bytecodes, dynamically reallocation of hash tables, and the actual execution stack are the fundamental structures of a Virtual Machine, and are effectively uncompilable without a direct pointer arithmetic. In their textbook on systems programming, Bryant and O'Hallaron (2015) note that high-performance system software would also need to have an interface to the hardware abstraction layer, not via the safety nets that typically provide application developers with protection against memory addresses. The C language provides this free access. The MOON Virtual Machine can request, resize and free exact amounts of memory in bytes, and that is through the manual invocation of standard allocation routines (malloc, realloc, and free). This grants the compiler the freedom to put instructions tightly next to one another to reach as much locality of the CPU cache as possible in the form of a blistering fast and simultaneously highly optimized execution engine which is also portable to any operating system with the help of a standard GCC toolchain. 

#### **3.3 High-Level System Architecture** 

On a bigger scale, the MOON programming language is applied on a two-pass and decoupled pipeline system. The contemporary compiler theory holds that the environment must be decoupled, giving a rigid distinction of concerns, which is a pure isolation of the human readable syntax of the frontend to the machine executable logic of the backend (Aho, Lam, Sethi, and Ullman, 2006). This architectural implementation is clearly crafted to make sure that the execution engine does not squander its computational resources to learn English grammar and also makes it more onerous on the parser to restore CPU state and memory area. 

The MOON architecture is very modular with the compilation process and execution being divided into two operational passes. One is that assuming the implementation of the language has another execution environment in the future the frontend is not modified at all but the reverse is true, in case the syntax has to be modified there is no necessity to make changes to the Virtual Machine backend (Cooper and Torczon, 2011). 

The system passes through the following two passes in the pipeline: 

Pass 1: The Frontend (Analysis): This is the pass that is all to do with the human intent and structural validity. The front end is not performing any computations or logic execution of the user. The data flow is done in rigid sequence conversion: 

- Source Code to Lexical Analyzer: The pipeline takes some time before the entry of the first character of the raw source code as a continuous stream of characters. The Lexical Scanner then reads this string and processes it mathematically to weed out all unjustifiable formatting (e.g. whitespace) and is left with the raw characters in a discontinuous series of acceptable units of valid language. 

- Token Stream to Pratt Parser: The output of the scanner is a linear Token Stream. The syntax analyzer immediately gulfs down such tokens and parses them on a Top-Down Operator Precedence (Pratt) basis. 

- Compiling the Abstract Syntax Tree (AST): The Pratt parser tests the rules of the MOON grammar by dynamically executing the operator precedence rule, and reorganizes the nonhierarchical stream of tokens into a hierarchical Abstract Syntax Tree. The pipeline halts at this point to allow malformed logic to be executed on the execution engine in the event of a syntax error in the source code. 

Pass 2: The Backend (Synthesis and Execution) Once the structural integrity of the logic supplied by the user has been successfully verified by frontend the work of frontend is completed, and it is the backend that starts. All that the work of this pass involves is to encode the abstract representations and produce optimized machine code. 

15 

- AST to Code Generator: The compiler back end starts a post order walk of the Abstract Syntax Tree. As it passes through every node it logically flattens hierarchical and tree-based expressions to a dense and linear sequence of instructions. 

- Bytecode Synthesis: Code Generator creates a final product; in this case, a very compact representation of a machine instruction in single-byte form; specific to the internal architecture of MOON, or a pool of variables that is always constant. 

- Virtual machine run: This completed compiled by-code is fed into the Virtual Machine. The VM is a software-simulated processor that initiates a compound of fetch-decode- execute to implement the bytecode instructions on its own autonomous memory stack to essentially implement the algorithmic rationale in the user algorithm. 













































_Figure 1: High-level Hierarchical Overview of the conversion of source code to execution by the_ 

_Virtual Machine_ 

16 

#### **3.4 Language Design and Formal Grammar** 

Even a compiler itself has to be a strictly, mathematically specified language before even it can be physically realized to transform the text into executable machine code. A computer is not able to understand ambiguity effortlessly although simulation of a fluid grammar is the prime objective of MOON. Thus, the initial task of the approach was a very rigid building of a Context-Free Grammar (CFG) which is optimal in translating natural language structures to deterministic and unambiguous mathematical rules (Chomsky, 1956). This section presents the design philosophy of the theoretical design, the lexical taxonomy and syntactic formalization of the MOON programming language which is abstracted absolutely with respect to the underlying C implementation. 

##### **3.4.1 The Design Philosophy of Natural-Language Syntax** 

The symbolic boilerplate is far more popular in traditional systems programming languages. An example of this is that a declaration, an assignment and a return of a variable in the standard C would look as int x = 5; x = 10; return x; This extremely convenient representation to the compiler would impose an artificial mental load on new programmers (MacLennan, 1999). Architectural philosophy MOON does the reverse: syntax is made oriented towards human understanding, with the heavy programming implemented in the parser. In MOON, the above C-code is grammatically translated to enable x to assume the value of 5, and then x is set to 10, and then x is provided. The language imitates both the thinking patterns of the human mind by replacing fixed symbols with semantic keywords (let, be, add, to, give), and provides it with the strict, mathematical constraints required by a compiler. 

##### **3.4.2 Lexical Specification and Primitives** 

MOON is lexically defined, and character sets and valid primitives accepted by the language are: 

- Standard data types are a floating-point number with double precision, Booleans (true, false), and a given null state (nil). 

- Strings and Interpolation: The literals of strings are included in regular quotations. However, the language has dynamically interpolated built-in semantically-structured textual string interpolation (in conceptual form as "Hello `name`!") such that expressions may be calculably re-compiled at the text border without necessarily hand-coding a program and a string into one. 

- Collections: MOON presents 2 significant data structures of complex information at the lexical level. Bracket notation ([1, 2, 3]) is used to specify lists and brace notation ( {name: "Munachi," age: 22}) is used to specify Dictionaries (hash maps). In order to have a restricted memory assessment at the time of compilation both of the collections have size limits because a collection is restricted in size to 255 elements in a declaration, the maximum cardinality of the objects. 

In addition, the lexical vocabulary actively substitutes old logical operators with English ones. The grammar imposes and, or, and not to the cases of &&, || and !. 

##### **3.4 3 Syntactic Formalization (Context-Free Grammar)** 

Deterministic grammar rules were used so that the fluid pseudocode of MOON could never be forced to contend with an ambiguous linguistic edge-case, and was conceptually based on Extended Backus-Naur Form (EBNF) structures (Appel, 2002). 

##### _Variable Declarations and State Mutation_ 

The grammar after the declarations has a strictly adhered rule of the form of the letters let <identifiers> be <expressions> . This means that it supports both single variable assignment and out of the box multiple-assignment (e.g. let a,b be 1,2). The formal grammar has the arity (count) of identifiers constant to the arity of the expressions to make a valid statement. The mutation rule of the form of state to expressions is also a set <identifiers> to <expressions> rule. Another syntactic rule that MOON introduces is the Accumulator: add <expr> to <identifier> rule. This reduces 

17 

conceptually to a standard binary addition on the grammar level hence the user is not bothered with repeated assignments. 

##### **_Advanced Object Access (L-Values)_** 

In the language theory, an L-Value is an expression which compiles to a specific memory address. MOON generalizes traditional L-Value grammar by having three accessors of the semantically equivalent ones, in addition to normal identifier access (list). Normal subscript notation (list[0]) may be used by a programmer, or any of the more dynamic dot-indexers where the right-hand term is an expression, not a constant property (list.i), or a still more natural sounding possessive accessor (list's length). These three structures are mathematically reduced to one behavior of access to properties by the grammar. 

##### **_Control Flow and Loop Constructs_** 

Block structures of MOON do not contain curly braces, they contain a terminating end keyword. Conditional grammar (if <condition> else:<block> end) has a colon to mark the beginning of a multiline block, however it may be written in one line by skipping the colon and writing the statement. One more addition by MOON is the unless keyword which adds expressiveness to the grammar with the grammar itself evaluating it as the mathematical inverse of the keyword if. The loop structures resemble these (e.g. standard while logic and its inverse until (while sum < 10; until x + 3 is 50) and an extremely versatile for loop which can be expressed in natural language e.g. for each item in list or for each i 1 to 10). This can be interrupted by use of break and skip to interrupt the execution of a loop. 

##### **_Statement Modifiers (AST Inversion)_** 

One of the features of grammar that has been developed the most in theory within MOON is Statement Modifiers. Based on other languages like Ruby, MOON allows the addition of control flow to the end of an action statement (e.g. give x if y > 5 or add 1 to player's score if not dead). This is not in a linear form grammatically. Before the evaluation of the action, the condition must be checked. The language specification is an architectural inversion, the trailing condition is a conceptual implementation of a preceding action, a simple statement is turned into a conditional branch, and the boilerplate of a literal if block is not needed. 

##### **_Phrasal Functions and Signature Interleaving_** 

The C-style standard grammar of functions has the name of the function and the arguments divided, and must use the form max(a, b). MOON has completely forgotten about this and adopted Phrasal Functions. The syntax is similar to Objective-C and Swift and allows mixing parameters directly into the signature of the naming of the function. The formal expression of a function declaration is similar and it appears as: let max of (x) and (y): <block> end (max of 5 and 50), the grammar requests the parser to dynamically match the sequence of various given arguments with a mangled signature string ‘ _max_of$1_and$1’  (_ where _ stands for the space before a label, and $n is the number of arguments it expects). 

##### **_Contextual Operations (Sticky Subjects)_** 

To eliminate redundancy of more complex Boolean logic, MOON grammar employs Sticky Subjects in grammar. In the case of the chained evaluation which is written by the developer (such as x > 5 or is 10), the traditional parsers would fail and the expectation was to have the left-hand side of the < symbol. MOON has grammar official support of context-sensitive operator chaining. The formal specification states that there are relational operators (is, ==, <, >) which store the lefthand operation temporarily in the state of the language evaluation. The grammar uses dynamically the left-hand value of the subject stored in the cache when the next operator does not have it, thus making it possible to have perfectly fluid English. 

18 

#### **3.5 Phase 1: Lexical Analysis (The Scanner)** 

The compilation pipeline actually begins with Lexical Analysis. Generally, a computer considers a source file as a stream of unstructured, nonstop, and raw character data. Before it can have the grammatical structure of the MOON language validated, however, this stream of characters has to be rationally broken into small, mathematically digestible units of logic, known as tokens (Aho, Lam, Sethi, and Ullman, 2006). MOON scanner was developed based on the architectural limitations of the language and a linear and single-pass Finite State Machine (FSM). In this section, the theoretical algorithms behind the scanner, its sliding window architecture, memory efficient, its deterministic key word resolution and its more enhanced context sensitive tracking of the string interpolation are outlined. 

##### **3.5.1 The Sliding Window Architecture and Zero-Copy Tokens** 

A sliding window algorithm is the primary concept of the lexical analyzer, which is managed by two logical pointers. As the scanner is analyzing the code, it uses an initial marker to designate the beginning of a potential language item (a lexeme) and a moving marker that character by character (Cooper and Torczon, 2011). When the advancing marker reaches a terminus (usually a space, a punctuation mark or a mathematical operator), then the state machine ends. The fragment of subtext that exists between the markers of the beginning and the present is a complete lexeme. At this stage, memory management is a very important systems engineering issue. It would be astronomical on the cost of compilation assuming that the scanner was dynamically reallocating new heap memory to accommodate each and every word or number that it met. MOON scanner uses a data model that is defined as zero-copy to address this. The scanner does not make a copy of the string contained in the Token structure created by the scanner. Instead, it simply stores the address of the beginning marker of the memory and the numerical size of the lexeme. The scanner is able to execute hundreds of thousands of tokens/sec by simply scanning the original code in memory and virtually at zero allocation overhead. Finally, all the tokens are labeled with the line number during that time so that they can be used in the later stages of diagnostic error reporting to form a stack trace. 

##### **3.5.2 Keyword Resolution (The Trie Data Structure)** 

One of the most computationally complex processes of a typical compiler frontend is the disentangling of user-declared variables (e.g., health or player) and reserved language keywords (e.g., let, give, unless and while). A particular lamentable architectural hack is to merely read an identifier, search through an array of all the thirty keywords used by MOON and match it against each one in full. Algorithms-wise, this implies that the time complexity is on the order of O(N x M) and the larger the files the more sluggish the compilation process would be. The MOON scanner is based on a Deterministic Finite Automaton (DFA), conceptually equivalent to a hard coded Prefix Tree (Figure 1), or Trie, to undergo the resolution of keywords to be mathematically optimal (Hopcroft, Motwani, and Ullman, 2006). The state machine is also deterministically branched on character sequences as opposed to whole strings. 

19 









































_Figure 2: Keyword Trie with double-circled nodes showing accepted keywords._ 

##### **3.5.3 Context-Sensitive Lexing for String Interpolation** 

The simple hand-written scanners lack the memory of the past states. Nevertheless, the MOON design philosophy can be said to favor dynamic string interpolation. A scan of such will require the scanner to pause scanning a string temporarily, then resume scanning a normal code (the name variable), then resume scanning the string. 

To achieve the same, the MOON scanner is based on a high-tech theoretical principle known as the Context-Sensitive Lexing (Scott, 2015). An interpolation Depth counter is inside the state machine. When the scanner meets an opening quotation mark, then the scanner starts in string mode. And when it comes upon an interpolation delimiter (a backtick `), it increments the depth counter, opens a hole in the string and produces a TOKEN-STRING-OPEN token. As the depth counter is not equal to zero anymore, the scanner temporarily goes to scanning the ordinary syntax of MOON and at this point the mathematical expressions and variables can be scanned normally. Upon receiving the closing back tick, the scanner checks the depth counter and decreases it and reenters into string mode where it emits a TOKEN-STRING-MIDDLE or TOKEN-STRING-CLOSE depending on the whether it is currently has a string after it or it is at the end of the string. This tracking is stateful and puts the burden of the string interpolation entirely on the scanner and the downstream parser is entirely clean and context free. 

##### **3.5.4 Lexical Optimization and Noise Reduction** 

Lastly, in the scanner the main optimization filter of the compiler is represented. The downstream Pratt parsing algorithm is based on a pure, uninterrupted flow of mathematically meaningful logic to construct the Abstract Syntax Tree. Human readable source code is necessarily non semantic noise: spaces, tabs, carriage returns, and developer comments. In MOON, the comments are indicated by a grain of two at signs (@@). In case these elements were sent to the parser, they would be subjected to dozens of grammatical rules which would only be used to disregard them. Thus, the scanner is an active noise reduction layer. On seeing the whitespace or a comment delimiter the state machine enters into a consumption loop that quickly pushes the current marker to the end of the line without ever producing any token. When the lexical analysis phase is complete, the source code is already logically cleared and only the concentrated operational information is needed to carry out the syntactic validation. 

20 

#### **3.6 Phase 2: Syntax Analysis (The Pratt Parser)** 

##### **3.6.1 The Abstract Syntax Tree (AST) Data Model** 

The most significant architectural paradigm shift in the compilation pipeline is the one between lexical analysis and syntax analysis. The scanner output is a one-dimensional array of tokens, the result of which is linear and flat. This array is logically correct and contains no formatting noise, but this does not give it at all the context of structure and precedence of operations (Aho, Lam, Sethi, and Ullman, 2006). The compiler cannot execute a flat list of the symbols like [x][+][3][*][y][<] [50] yet as the linear sequence does not dictate the sequence in which addition or multiplication should be executed first. In order to do away with this, the stream of tokens that is one-dimensional must be coded into a many-dimensional, topological graph known as the Abstract Syntax Tree (AST). The structural logic of the source code is unambiguous and it is absolute, the AST. In this model, all the operations, variables, and control flow mechanisms are modeled as an independent "Node" and branches establish the rigid mathematical sequence of evaluation (Parr, 2010). 















_Figure 3: The Abstract Syntax Tree model showing precedence of operations. Nodes are evaluated bottom-up_ 

On the conceptual level, the MOON AST is regulated by a rigid dichotomy that is well-known in the history of programming languages: it is the separation of Expressions and Statements (Scott, 2015). 

##### **_Expression Nodes_** 

An expression is any syntactic expression, which can be mathematically evaluated to a concrete value. These are: within MOON AST architecture, they are: 

- Literal Nodes (representing raw numbers, strings, Booleans and nulls) and Variable Nodes (representing a memory fetch). 

- Binary nodes (examples of standard operations such as addition or equality), Unary nodes (examples of negations such as -5 or not true), and Logical nodes (examples of shortcircuiting and/or behavior). 

- Complex Accessors: Subscript Nodes (array access via Moon's possessive (s) notation), Property Nodes (object access via the possessive (‘s) notation in Moon), and Range Nodes (generating a mathematical sequence such as 1 to 10 by 2). 

- Execution Handlers: The Phrasal Call Nodes are very specialized (that are taking the mangled signatures and interlaced arguments of the natural-language functions of MOON). 

##### **_Statement Nodes_** 

Conversely, statements do not generate values, their sole purpose is to generate side effects, e.g. mutation of memory, declaration of scopes, or redirection of execution (Louden & Lambert, 2011). The blueprint of the structure consists of: 

- State Management: Declaration Nodes (single or multiple variable declarations), Set Nodes (Renewing the memory of complex L-Values and mathematical accumulators). 

21 

- Decision Nodes: If, Unless Nodes (controlling conditional branching and statement modifiers), 

- Iteration Nodes: While Nodes, Until Nodes, and For Nodes (controlling iterative loops). 

- Execution Routing: Grouped statements (represented by Block Nodes), implementation of a callable routine (represented by Function Nodes), and the representation of the return command (represented by Return Nodes), and loop terminators (represented by Break Nodes and Skip Nodes). 

##### **_Bidirectional Node Tracking_** 

Abstract Syntax Trees tend to be defined as strictly downward-pointing Directed Acyclic Graphs, all of which are represented in this way. The parent node has knowledge of its children and the child has no knowledge of its parent. Each child node of the MOUN data model is linked with their that go up the structural tree. This bi-directional sense implies that the compiler will be able to maintain strict context without having to use complex and external tracking arrays. In case of a local failure that takes place halfway through a highly nested algorithm, e.g. due to an invalid access to memory in a conditional branch inside a loop, the concerned node can mathematically compute its structural enclosure by walking its parent links. This two-way graphing, theoretically, is vital to generate very contextual stack traces in case of error reporting and to perform dynamically to scoping of variables in case of compiling the code. 

##### **_Abstracting Complex Payloads_** 

The main characteristic of Abstract Syntax Tree is the absence of syntactic noise. The tree is a prototype of the grammar, but of the mathematical purpose. This is most evident in the notions of complex data payloads as dynamic arrays (Lists), hash maps (Dictionaries) and String Interpolation in MOON. Brackets and commas will be displayed by lexical scanner when a user types such a list as [1, 2 + 3, x]. The AST completely abandons these diphthongs. Instead, the parser generates a single and unique List Node. The conceptual payload of any node has no commas; it contains only a parallel array of child Expression Nodes, and a number which denotes the number of items in the array. Similarly, a Dictionary Node is an array of colons and braces that is swept under the carpet, in the spirit of a perfectly parallel array of nodes one containing the evaluated keys and the other containing the evaluated values. Interpolation Nodes are also based on the same architectural abstraction, except that instead of representing a dynamic string as a sequence of quotes and backticks they represent it as a sequence of pairs of Literal Nodes and Evaluated Expression Nodes. The AST enables the downstream compiler backend to be purely concerned only with the pure, concentrated algorithmic logic by eliminating the lexical artifacts. 

##### **3.6.2 Implementing Top-Down Operator Precedence (The Core Engine)** 

The compiler frontend must process the very dynamic and natural-language statements of the MOON programming language, so it is based on Top-Down Operator Precedence algorithm. To know about the architectural need of this particular engine, we need to first look at the theoretical constraints of the traditional interpretation systems of parsing systems in light of their critical limitations when trying to apply them to complex, English-like syntax. 

##### **_The Recursive Descent Bottleneck and Left-Recursion_** 

The Recursive Descent parser is the standard of the hand-written compiler frontends. In a pure recursive descent structure, each non-terminal symbol of the formal grammar is directly represented by a programmatic subroutine (Nystrom, 2021). Although this top-down approach is very intuitive to the analysis of inflexible, structured block statements (i.e. an if or a while block), it creates drastic bottlenecks when analyzing inflexible mathematical or fluid infix expressions. To implement operator precedence, such as to make sure that multiplication is done before addition, in standard recursive descent the precedence levels must be literally coded as part of the grammar structure of the language. As a result, the compiler has to make a series of series of calls to enormous numbers 

22 

of nested functions (e.g. expression calls equality, which calls comparison, down to term, factor, unary and finally primary) to convert a simple integer literal such as 5. With a very expressive language such as MOON, such kind of structural bloat adds insane, unneeded overhead, filling the call stack of the host machine, and then it has not even evaluated any logic (Bendersky, 2009). 

Moreover, the classical recursive descent has a problem with left-associative operations, including standard subtraction or division (e.g. A - B times C). Trying naively to model left-associativity in a naively written recursive descent parser will lead to a deadly left-recursion, in which a rule of the grammar recursively calls itself, without ever actually consuming a token, due to which a stack overflow is lethal. 

##### **_The Pratt Architecture: A Token-Centric Paradigm_** 

To avoid the recursive traps of the traditional methodologies the MOON expression parser does not use pure recursive descent but instead uses a method that was first proposed by Vaughan Pratt. The basic idea behind Pratt Parsing is the reversal of the classical compilation paradigm: instead of being grammar-centric, the architecture is entirely token-centric (Pratt, 1973). In this type of model, the parser is not based on the heavy nesting of subroutines to implement the grammar. Rather it applies a theoretic, centralized parsing table. In this table, each type of token in the MOON vocabulary is assigned three mathematical properties, which are considered to be critical independently: 

1. Prefix Behavior (Null Denotation): A conceptual rule defining what the token means if it appears at the very beginning of an expression. For instance, the minus token (-) behaves as a unary negation (e.g., -5), while an identifier token (x) fetches a variable. 

2. Infix Behavior (Left Denotation): A rule defining how the token behaves if it appears in the middle of an expression, mathematically binding to the data on its left. For instance, the plus token (+) takes the left-hand value and adds it to the upcoming right-hand value. 

3. Binding Power (Precedence): A strictly defined, numeric weight representing how aggressively the token binds to adjacent expressions. For example, a multiplication operator is assigned a higher binding power than an addition operator. 

Software architect Douglas Crockford wrote a token-based program to reprint the algorithm to interpret JavaScript and said it offers unmatched flexibility to dynamic languages, even though it is trivially efficient to run (Crockford, 2007). 

##### **_The Core Evaluation Loop_** 

The real beauty of the Pratt architecture is the central execution loop which completely unravels the deep recursion tree into a small, highly performing mathematical loop. When the MOON compiler starts to evaluate an expression, it reads the initial token and starts its corresponding prefix behavior, which immediately produces a first AST node, the left-hand one. The parser then comes under an endless while loop. This loop merely determines the numeric binding strength of the current context of execution compared with the next token in the sequence. When the binding power of the next token is strictly larger than the current situation, the token is greedily consumed by the parser and its infix behavior (turning the current left-hand node into its infix behavior) is triggered, and the left-hand node replaced by the resulting AST structure of the infix behavior. To give the example, in the case of parsing A + B / C, the loop processes A and comes to the plus sign, and starts to process the right-hand side. It then sees the '/'. This means that since the binding power of the '/ is greater as compared to the addition sign, the loop recursively calls upon the binding power of the '/, completely combining B / C and then returning the resultant node to the addition operation. 

23 

The complex nested expressions are automatically and tidily solved using this algorithm. More to the point, it solves the left recursion problem natively and does not need to implement awkward grammar rewrites as the numeric threshold prevents the loop, mathematically, to consume tokens of equal precedence in a right-associative way. 

##### **_Extensibility to MOON's Pseudocode Syntax_** 

One of the reasons why the unusual, English-like syntax of MOON is computable is the application of the Pratt algorithm. The architecture makes everything an infix operator with a binding weight and, therefore, MOON is capable of realizing very sophisticated accessors with zero structural friction. An example is that the possessive apostrophe (‘s) in the expression player s health, the dot indexer (list.i), and the range generating expressions (1 to 10) are high-binding-power infix operators modelled by the parser. The central loop also has the same consumption as a normal multiplication symbol, and sends them to special AST generation rules. This is an architectural choice that means that the language can dually increase its vocabulary of natural language indefinitely without ever breaking the underlying parsing engine. 

##### **3.6.3 Advanced Syntactic Desugaring and AST Inversion** 

The other fundamental architectural principle of compiler design is that, much as the front-end code of a language can and must describe the cognitive needs of the human programmer, the implementation engine in the back-end must be as mathematically simple and orthogonal as possible. The generated Instruction Set Architecture (ISA) would be a vastly oversized and highly ineffective were the Virtual Machine to have known all the variants of the English-like phrasing of MOON. To compensate the lack of expressiveness in the frontend and the simplicity in the backend, the MOON compiler applies two advanced topological manipulation operations to construct the Abstract Syntax Tree: Syntactic Desugaring and AST Inversion. 

##### **_The Idea of Syntactic Desugaring_** 

In programming language theory, syntactic sugar is high-level syntax that is designed so that it can be easier to read and comprehend by the human developer, without increasing the computational power of the language (Landin, 1966). A good illustration of such philosophy is the add x to 5 statement in MOON accumulator. Rather than having the compiler backend deal with quite a specialized accumulation-of-memory operation, the Pratt parser deals with this form by structural normalization (sometimes called desugaring). The grammatical pattern is known when the add keyword is met by the parser. It de-codes the value to be added (5) and the address to which the target data should be stored (x). 

The parser is dynamic and rather than giving a special Add Node, it gives up the logic and translates it into primitives. It conceptually replicates the target memory address, forms an ordinary mathematical Binary Node indicating addition and implements the entire expression in a typical Set Node (an assignment operation). Therefore, in the case of the developer writing add 5 to x, the AST is set x to x + 5. Through this transformation at the parser, the Code Generator and the Virtual Machine do not in any way know anything about the add keyword; they compile and run it just like any other variable assignment. This ensures that there is a significantly reduced and an optimized and minimal background execution environment (Sebesta, 2012). 

##### **_AST Inversion & Statement Modifiers_** 

The support of Statement Modifiers is one of the advanced parsing features that have been introduced in MOON. As an inspiration of massively expressive writing languages, MOON permits control flow logic to follow an action statement, giving it a lax syntax such as give x when y > 10 or add 1000 to balance unless rich. This presents a difficulty computationally. Normal LL(1) and Pratt parsers process tokens left-to-right. The inputting read give x and immediately, it encounters a full, grammatically correct action and an entire terminal Return Node is constructed. This is because the 

24 

parser is only revealed to the if token after the full construction of this node. Other compilers solve this problem by relying on recursive backtracking, undoing the state of the parser, uncreating the node under construction and recompiling back to the beginning which is catastrophically exponential in time (Aho, Lam, Sethi, and Ullman, 2006). MOON parser makes no attempts of backtracking, but uses an AST Inversion. 

The generation of the current statement will be halted by the parser upon encountering a trailing conditional modifier (the modifiers may be if or unless). It dynamically allocates a new and empty structure, Conditional Node (an If structure) in memory. It then decomposes the next predicate expression (y > 10). Finally, the parser makes a change: the action statement that has already been built (the give x node) is demoted by the parser and deprived of its root status, and inserted internally as the valid then-branch of the newly created Conditional Node. 

Also, the inversion is compounded if the trailing modifier is the unless keyword. Before the predicate is injected in the conditional structure by the parser, the predicate is placed in a Unary Negation Node (Not) and the predicate is then transformed by the parser into the form 'unless rich' to the form 'if not rich'. 

Under this structural inversion, the parser is quite expressive and no additional rules are required on the back-end execution. Sending the code to a compiler back end makes the inverted AST structurally the same as a typical, multi-line if block (Scott, 2015). 

##### **3.6.4 Contextual Operator Chaining ("Sticky Subjects")** 

The fact that typical compiler frontends use Context-Free Grammars (CFGs) is one of the inherent limitations of these frontends. A context-free parser assumes a sequence of tokens in their local adjacency only, but does not remember anything about elements that were already processed, except the one that is currently pushed on the call stack (Chomsky, 1956). Though this stiff locality is so foregone to parsers, it is actually the active opposite to the flowing, elliptical nature of human communication. This paradox is speculatively representational in the event of complex Boolean reasoning. 

##### **_The Grammar Problem of Elliptical Logic._** 

An English developer would then write; if x is greater than 1 and less than 100. A reader who is a human being instantly notices that the topic of the greater-than and less-than comparisons is that of x. It is however completely unsupported by a context-free parser (such as a conventional Pratt compiler or LL(1) compiler). When the expectation of the parser comes to the and keyword, it expects a full expression on the right-hand side. The less-than operator (<) is the next operator. Because, since, is a binary operator, the internal architectural policies of the operator dictate that it must have a left-hand operator. Failure to find any results in a fatal syntax error of the parser. It lacks the situational knowledge to retrospectively scan over the logical and to realize that x was the target object (Parr, 2010). Thus, when using traditional languages, the developer has to satisfy the CFG by developing the redundant logic: if x is bigger than 5 and smaller than 10. 

##### **_The State-Caching Solution_** 

In order to bridge the human ellipsis and machine arity, the MOON compiler introduces a more advanced theoretical instrument known as the Contextual Operator Chaining that is processed by the internal register named the Sticky Subject. It consists of the algorithm of adding to the conventional stateless evaluation loop of the parser a very local short-term memory register. As the primary relational operator is considered, the evaluator is interrupted when the evaluator of the operator is the equivalence (is, =) or inequality (<, >, <=, >=). This stateful register is written by the parser with a conceptual reference to the considered left-hand expression node and the standard 

25 

binary AST node is then created. This manifestation of the left is now said to be sticky; it has become the silent element of the contemporary logical series. 

##### **_Node Injection and Implicit Node Cloning._** 

When the parser subsequently encounters an orphaned relational operator (an operator that has no obligatory left-hand side such as the alone < in and < 10), this does not necessarily result in a syntax error. It examines the Sticky Subject register. In case there is an active subject of the state of the parser, the engine dynamically interrupts the failure and initiates an implicit structural resolution. 

In conventional compiler front-ends Abstract Syntax Trees (ASTs) are represented as strict trees, i.e. a node can only have a single parent. This makes it a Directed Acyclic Graph to make use of the same instance of the node to serve multiple parents by altering the assumptions of traversal and ownership. In spite of the fact that the parsing of DAG representations is intentionally performed at the later stages of optimization, the incorporation of shared substructure during the analysis can complicate the analysis and transformations on the basis of tree properties. A duplication is done in the parser to ensure that the mathematical purity of the tree is preserved. It copies-on-the-fly the subject node which is in the cache- cache memory, and structural payloads-recursively copies its underlying memory footprint and safely inserts this new minted copy in the AST as the absent lefthand operand. 

In this transparent structural cloning, the parser mathematical generalizes the case of x being greater than 5 and less than 10 to the case of (x > 5) and (x< 10) all the way down in the background. The luxury of writing code of a very fluent nature is assigned to the human developer, and binary-only operations of acceptable quality are assigned to the Code Generator and Virtual Machine. This architecture is a kind of applying complexity to the frontend analysis phase to the maximum of the expressiveness of the language, to the point where the predictability of the backend implementation is affected (Scott, 2015). 

##### 3.6.5 Phrasal Function Resolution (The Signature Automaton) 

The syntactic feature of the MOON programming language that is the most developed one is the integration of the Phrasal Functions. Trying to make the pseudocode as fluid as possible, the language design deliberately blurs the line between the invocations of the functions and the typical expressions. Nevertheless, this design, though human readable, starts a huge theoretical problem with the underlying parsing engine. 

##### **_The Parameter Interleaving Problem_** 

In classical programming languages, the functions are fully encapsulated. A definition has one continuous identifier with a parenthesis list of parameters after the identifier like calculateSum(5, 10). Since the identifier and the arguments are geometrical insulated, a standard parser can easily solve the invocation. MOON, in its turn, is based on the interleaving of parameters. A function is called in the form of a continuous English phrase, e.g. sum of 5 and 10. When such is fed to a standard Pratt parsing loop, a lethal syntactic collision is the result. The sum is read by the engine and it is registered as a variable. It then advances to 'of'. Since the operator of is not a universally reserved mathematical operation (such as + or multiplication), the context-free Pratt engine does not syntactically hold the operator of. It is considered an unexpected token gift and ends with a syntax error. The Pratt parser essentially fails to understand the fact that "sum" and "of" and "and" make up a unitary signature of operation (Scott, 2015). 

##### **_The Pre-Pass Hoisting Strategy_** 

The MOON frontend makes use of a multi-pass compilation structure to dynamically add phrases of user-defined functions to the vocabulary of the parser (Aho, Lam, Sethi, and Ullman, 2006). The compiler starts a fast lexical scan of the entire source file before the actual syntax analysis phase 

26 

which starts building of the Abstract Syntax Tree begins. In this pre-pass, the scanner actively scans out the declarations of functions. It finds a signature, such as sum of (x) and (y): and it does this by removing the structural labels (sum, of, and) and the parameter arity of those labels (arity of each step expected). This signature is registered by the compiler into a global state table and its scanner conceptually rewinds to the start of the file. The primary Pratt parser is methodically aware of all the custom phrasal functions in the program by the time it starts to run. 

##### **_The Signature Trie (Deterministic Finite Automaton)_** 

These hoisted signatures are not represented as flat strings to be able to store the signatures at highperformance O(1) resolution the first time through the main parsing pass. They are mathematically arranged into a Prefix Tree (Trie) that is in active mode as a Deterministic Finite Automaton (DFA) (Hopcroft, Motwani, and Ullman, 2006). The Signature Trie is composed of two types of conceptual nodes (Label States and Argument States) that represent the expected keywords and parameter boundaries respectively. 

The root word ‘sum’ is recognized by the parser which makes a query to the DFA. The characteristic expected execution is transformed into a series of transitions. The automaton leaves the root [sum] to label requirement [Label: of]. After which it passes into an evaluation state [Argument: Arity 1] which requires a single expression. It then moves to another labeling requirement [Label: and], and finally [Argument: Arity 1] is obtained. In a case where the DFA has entered into the terminal accept state, it dynamically compiles the whole phrase into a mathematically mangled internal identifier (e.g., sum_of$1_and$1) and has flawlessly bundled the interleaving arguments into a single Phrasal Call Node into the AST. 

##### **_The Pratt/DFA Synchronization_** 

The architectural denouement of this system is the way the DFA and the Pratt engine talk to each other without corrupted communication. Upon reaching an [Argument] state, the DFA has to call on the Pratt engine to analyze the mathematical expression (e.g., to evaluate the mathematical expression like 5 + 2 and then pass it to the function). The conflict of the boundary is as follows: when does the Pratt engine adopt the argument and hand-over the control to the DFA? In case of calculating ‘sum of 5 and 10’, the Pratt engine will scan the 5, but it may also attempt to scan ‘and’ to interpret it as a logical operator, thus stealing the token to the DFA signature. To address it, the methodology adopts a conceptual Blindfold of the cross-engine synchronization protocol. The DFA first scouts the future of its own Trie structure before the DFA calls the Pratt parser so as to know the next one it will need (‘and’ in this case). This expected label is mathematically hashed by DFA and pushed on a stateful stack which is very localized. 

This stack is respected in the design of the Pratt engine. When in its main evaluation loop, prior to it swallowing an infix operator, it examines the Blindfold state. In case the next token is equivalent to the hash of an anticipated DFA label, the Pratt engine will short-circuit. It breaks its expression loop, and in effect does not consume the token, leaving the execution control with the DFA once again. The DFA safely eats the and mark, de-steps its tree and goes on with the phrasal assessment. To make an expression with a label that is inside the signature of the function, the expression is put in parentheses (as in ‘place (5 to 50) to list’). This gracious synchronization is what ensures that complex and dynamic phrases in the English language can work seamlessly with the rigid mathematical order of operation without adding any parsing ambiguity or eventually necessitating costly backtracking algorithms. 

#### **3.7 Phase 3: The Compiler Backend (Bytecode Generation)** 

Since the Pratt compiler guarantees that the source code is mathematically correct and the structural integrity of the code is maintained by the same compiler, the compilation pipeline is the one that cuts across the architectural boundary between analysis and synthesis. The output of the frontend is 

27 

the Abstract Syntax Tree (AST), a much more descriptive and many-dimensional graph, easily and semantically verifiable by humans, but which cannot fundamentally run on a microprocessor. A computer, be it a hardware or software implementation, does not process the deeply nested graphical trees, but rather linear, contiguous streams of instructions (Wirth, 1996). Phase 3 is primarily charged with the activity of disaggregation of this tree in a systematized manner and encapsulating the abstract node of linguistic intent into a rich and highly optimized sequence of machine-readable instructions known as Bytecode. This portability implies that MOON execution engine never depends upon the native hardware of the host operating system, but at the same time it is almost as fast as the native hardware as far as its calculation speed is concerned (Smith and Nair, 2005). 

##### **3.7.1 The Instruction Set Architecture (ISA)** 

The Virtual Machine needs to have a rigidly defined set of permissible actions in order to run the compiled bytecode. This set of vocabulary is formalized and called the Instruction Set Architecture (ISA). Instead of use of Complex Instruction Set Computer (CISC) design which executes highly specialized and multi-cycle operations, the MOON compiler backend uses a Reduced Instruction Set Computer (RISC) philosophy. 

##### **_The Opcode (Operation Code)_** 

The basic unit of computation in the MOON ISA is referred to as the Opcode. In order to achieve maximum locality of the memory caches and throughput on the dispatch of instructions, all the basic operations the Virtual Machine can execute are highly compressed into a single eight-bit byte (which is why the name is given to it as bytecode). Since the host CPU reads memory in blocks of successive instructions, by limiting instructions to single-byte identifiers, the Virtual Machine can find, decode, and execute instructions at mathematically optimal throughput (Cooper & Torczon, 2011). 

##### **_Core Categories of the ISA_** 

The compilation generated code is conceptually divided into four major architectural domains: 

1. Mathematical and Logical Operations: One-byte instructions, which direct the Virtual Machine to pop items out of the memory stack, ALU (Arithmetic Logic Unit) level operations (e.g. addition, equality, negation of binary numbers), and pop the result back. 

2. Variable Access and Memory Management: Instructions that are used to fill active memory or to store the result of a computed result. It has special opcodes between fetching dynamically bound global variables and unresolved local variables which are resolved at a later stage. 

3. Control Flow and Jump Directives: The compiler does not create opcodes of if and of while. Rather, it produces low-level, absolute instructions in manipulating the instruction-pointer. The Virtual Machine has opcodes such as JUMP and JUMP-IF-FALSE, which cause the physical skipping of parts of the bytecode array, and mathematically emulates conditional branching. 

4. Execution and Scope Routing: Sophisticated opcodes used to manage the suspend of the immediate environment, including a call to a phrasal (CALL) or a given value (GIVE). 

##### **_Linearization via Post-Order Traversal_** 

The ultimate algorithmic problem of the compiler back end is the topological translation of a hierarchical AST tree into a one-dimensional array of opcodes. The Code Generator applies a recursive Post-Order Tree Traversal algorithm to attain this (Aho, Lam, Sethi, and Ullman, 2006). In a post-order traversal, the algorithm requires it to visit all child nodes of branches recursively and then it is only after that that it is permitted to process the parent node. A stack based Virtual Machine cannot do without this particular graphing algorithm. Since a stack architecture requires its operands to be actively loaded at the top of the memory stack when an operation is called, the 

28 

operands also have to be compiled initially. To illustrate, an expression that is entered to the compiler backend in the form of an AST that is equivalent to 5 + 10 does not generate the ADD command when the parent + node is encountered. Rather, post-order algorithm compels the compiler to take the left branch and it generates the bytecode to load 5 to the memory. It then goes through the right branch, which releases the code of loading 10. Lastly it moves back to the parent node and gives the ADD opcode. 

This recursive, top-down flattening of the tree is a simple process through which the multidimensional tree is flattened into an unbroken series of one-byte instructions. Even the hierarchical complexity of the MOON language is entirely removed by the time the code generation phase is finished, and all that remains is pure and ordered machine logic to be fed to the execution stack. 

##### 3.7.2 Bytecode Chunking and the Constant Pool 

Just as laid down in the previous section, the compiler flatten-outs the hierarchical Abstract Syntax Tree systematically to produce a linear sequence of single-byte code instructions. But an architectural question must be asked at once: how and whither is this unbroken flow of machine logic physically stored in memory prior to being run by the Virtual Machine? The backend is unable to allocate a fixed-size memory buffer because the compiler does not have certain knowledge of the overall size of a user script before the code generation stage, which means that the code generated size cannot be known deterministically. To address this, MOON compiler architecture proposes the principle of "Chunking" and is also directly related to an independent data structure called the Constant Pool (Nystrom, 2021). 

##### _The Architectural Concept of a Chunk_ 

In typical virtual machine books, Chunk is a dynamically growing, contiguous block of memory whose purpose is to contain compiled code. These instructions are then added sequentially to the active Chunk as the post-order traversal algorithm is executing its traverse of the AST and emitting opcodes. The Chunk uses the formula of amortized doubling on the mathematical basis in order to sustain O(1) complexities of insertion time without consuming too much idle RAM. Once the internal capacity of the Chunk is hit, the memory footprint will automatically grow by an exponential growth factor, and the copying of the existing instructions to the newly grown memory block will be safely done. The choice to hold the bytecode in one, contiguous array instead of a non-contiguous (such as a linked list) structure is a significantly planned optimization goal at the hardware of the host machine. Both spatial locality and L1 instruction caches are highly demanded by modern microprocessors (Bryant & O'Hallaron, 2015). With each byte of the constructed MOON script literally being in physical proximity to the following byte in the RAM, the Virtual Machine can sequence the Instruction Pointer linearly, without ever causing a costly CPU cachemiss. 

##### **_The Operand Bottleneck and The Constant Pool_** 

Though a single-byte opcode offers the advantage of the fastest possible execution, it has a profound mathematical constraint, namely, an 8-bit byte can only physically form a set of 256 different values. This limitation poses an architectural bottleneck instantly when it comes to user information. When the compiler receives the statement, let balance = 10000, or has the statement print Hello World, the compiler has a severe bug. It does not have the power to force either a 64-bit double-precision floating point number, or even a character string of variable length, directly into an 8-bit executable program stream, without entirely ruining the alignment of the bytecode (Patterson & Hennessy, 2013). In case the execution engine tries to interpret a string as an op-code, the Virtual Machine would crash disastrously. 

29 

In order to overcome this hardware drawback, the MOON architecture provides a "Constant Pool" on top of each of the compiled Chunks. Constant Pool is a secondary and isolated dynamic array which is specifically used to store large data payloads. The idea is highly reminiscent of the design of execution environments of enterprise grade, most prominently, the Java Virtual Machine (JVM) that uses a centralized constant pool of runtime to handle method areas and string literals (Lindholm, Yellin, Bracha, and Buckley, 2014). 

##### **_The Storage and Retrieval Mechanism_** 

In the AST, when the compiler is presented with a large literal (a string, or any large integer or floating-point number), it is not coded to emit such a value into the main stream of the bytecode. Instead, it puts the payload safely in the Constant Pool. The pool then returns a numeric index that is lightweight and numeric (e.g. index 4) that is the precise position of the payload in the array. 

At this stage, the compiler will issue a special instruction e.g. OP-CONSTANT into the main Chunk, the lightweight index byte (e.g. 4) will be emitted immediately after it. In execution, the Virtual Machine picks up the OP-CONSTANT instruction, and to realize that it must load a value, reads the next index value, and utilizes it to obtain the gigantic data load securely out of the Constant Pool. By using this exquisite architectural division, the MOON compiler makes the instruction stream of the executable form a symbolically precise, extraordinarily-compact and mathematically-independent object whose contents, the data it operates on, exhibit a variable length. 

##### **3.7.3 Scope and Symbol Resolution** 

Although the Constant Pool perfectly addresses the hardware constraints of storing the raw data such as strings and big numbers, it fails to address the conceptual state management problem. When a user types in such expression like add 5 to balance, the compiler cannot simply add the word balance to the compiled code. The Virtual Machine itself is incapable of comprehending human words; it only comprehends the memory addresses (Aho, Lam, Sethi, and Ullman, 2006). The symbol resolution process of mapping identifiers that are legible by human beings to hard and fast machine-memory addresses is called Symbol Resolution. This is significantly split into two fundamentally different theoretical mechanisms, late-bound global resolution and zero-overhead local resolution, in the MOON compiler architecture depending on whether the variable is lexically local or global. 

##### **_Global Variables and Late Binding_** 

In case a variable is defined at the highest level of a MOON script, it is considered to be a Global Variable. The global scale demands a very dynamic architecture design since these variables need to be maintained throughout the entire program life cycle and have to be available anywhere including within the confines of isolated functions. In order to handle this, the Virtual Machine uses a background state of a global Hash Table (key-value dictionary) in the Virtual Machine. But, the compiler back end does not know the specific address of the variable at run time in the memory hence cannot know the exact address of the global variable at the compilation stage. Thus, it is based on the principle of a so-called late binding (Scott, 2015). 

The compiler uses the architecture of Phase 3.7.2 when creating a global variable assignment in the form of a bytecode. It takes the name of the variable (e.g. balance), and uses it as a literal of a string and injects it into the Constant Pool. It then adds a DEFINE-GLOBAL opcode to the Chunk, and then the Constant Pool index. During the running, the Virtual Machine will read the opcode, fetch the string in the Constant Pool, hash it, and access the dynamic Hash Table. Though very flexible, this late-bound resolution comes at a modest execution cost because of the cost of executing the algorithmic complexity of hashing strings at run time. 

30 

##### **_Local Variables and Zero-Overhead Stack Offsets_** 

In order to realize the highest computational throughput, the large percentage of variables in a wellstructured program are locally scoped, residing inside block statements, loops, or function bodies in their entirety. Local variables (which have strict and predictable lifespans due to Lexical Scoping) can be bypassed by the MOON compiler as a block, and the compiler can execute them with a zerooverhead execution footprint (Nystrom, 2021). 

Instead of assigning the resolution to the Virtual Machine the compiler provides definitive resolution to local variables at compile-time through an array of simulated execution. In a case when the frontend crosses into a localized block (e.g., a conditional statement) and it is met with a declaration, e.g., let x be 10, the compiler backend conceptually stores the identifier x into its own simulated stack. When the next line of code demands the value of x, the compiler would not produce a string-based look up instruction. It instead searches in a simulated stack array backwards to find x. After this, the compiler computes the precise geometric integer offset of the bottom of the current call frame (e.g. x lies directly at stack slot number 3). A compiler then produces a muchoptimized GET-LOCAL opcode and the integer offset value (3) is produced directly after the opcode. When this bytecode is executed by the Virtual Machine it does not do any string hashing or dynamic lookups. It merely reads the integer and makes an immediate, O(1) index access to its physical memory stack (e.g. stack[3]). The architecture of the back-end by ensuring that the heavy mental burden of following the lexical scopes is completely removed by the compiler frontend, the back-end architecture structurally ensures that the manipulation of local variables is performed at the maximum possible speed possible by the host hardware, effectively replicating the performance properties of a language that is compiled statically, such as C (Cooper and Torczon, 2011). 

#### **3.8 Phase 4: The Virtual Machine Architecture** 

##### **3.8.1 Dynamic Value Representation (Polymorphism and Memory Models)** 

The architectural implementation of MOON of a human-readable script into executable machine logic ends with the Virtual machine. Nonetheless, even prior to the execution engine capable of executing a single algorithmic instruction, it has to address a basic structural paradox: the host environment is bound to strictly, stateless memory, and MOON is a dynamically typed language by its nature. 

##### **_The Typeless Memory Problem_** 

A dynamically typed language allows a programmer to change the state of a variable, which may be true/false, into a high-precision decimal value, and thence into a full-blown complex data representation, with no declaration of a formal type whatsoever. Although this gives the programmer a great deal of freedom in his cognitive capacity, it also presents the execution engine with an excruciatingly tough theoretical challenge. A simulated stack of execution is in the end simply a continuous, homogeneous array of uncooked memory blocks. The semantic context of this data is virtually thrown away when the Virtual Machine places data onto this stack. When the machine merely records the raw binary form of a floating-point value in a memory address, and attempts at a later time to read the same memory address with an expectation of a Boolean, it will be reading inaccurate bits and may cause disastrous logic errors. The architecture should have a way of encoding and decoding entirely dissimilar mathematical entities into the same geometric constraints and never losing their semantic content and contravening type safety (Scott, 2015). 

##### **_Type-Tagged Memory Abstraction_** 

In order to address this Typeless memory issue, the MOON execution architecture does not use the raw data insertion but instead relies on a hypothetical model referred to as Type-Tagged Memory Abstraction. With this paradigm, the Virtual Machine does not deal with raw data in any way. Rather it uses the form of a single, bipartite structure to model each individual atomic unit of information. A type tag is the first element of this structure which is a metadata explicitly stating the 

31 

current identity of the data (e.g. Boolean, Decimal or List). The second element is that of a shared memory envelope. This envelope is a mathematically determined fixed memory block or envelope that is geometrical and safely coincides with other native data representations. When the Virtual machine removes an item in the stack to perform an operation, the Virtual machine does not execute the math straight away. It initially questions the type tag. The engine ensures perfect type-safety by looking at this structural metadata at the microsecond of execution and dynamically dispatching the enclosed memory envelope to one of the possible arithmetic or logical subsystems without ever experiencing any type constraints (Louden & Lambert, 2011). 

##### **_Immediate vs. Referenced Values_** 

Although the type-tagged abstraction addresses the same issue of semantic identity, it presents a second complication of space. There are data types, like Booleans and decimals, which have a very strict and limited mathematical footprint. In contrast, there are the complex data formats such as strings, dynamic arrays, hash mappings; these are unbounded; meaning that they grow dynamically at runtime. Provided that the shared memory envelope on the execution stack were large enough to hold the longest conceivable string, the execution stack would take up an extravagant quantity of unnecessary memory, killing the cache efficiency of the CPU. The Virtual Machine architecture must provide a very high processing speed and maximum data density, and therefore enforces a firm separation of data routing: 

- Immediate Values: Simple data (numbers, Booleans, and null states) which are geometrically bounded are termed Immediate Values. Their memory footprint is guaranteed to be within the shared envelope, hence being stored in the primary execution stack as-is and in their entirety. To access them, zero indirection is needed and this makes them instantly accessible. 

- Referenced Values: Unbounded, dynamically sizing data is considered as Referenced Values. The Virtual Machine does not make an effort to push the data into the execution stack when the Virtual Machine produces a list or a string. Instead, the architecture forwards actual data content to some external dynamic memory pool (the heap). A lightweight, abstract reference is then loaded in the memory envelope of the stack and a spatial locator is loaded which refers to the external data. 

This is a very important architectural dichotomy. The main execution stack is uniform, densely packed and amazingly quick to run by conceptually partitioning data according to its spatial boundaries. It makes sure that the Virtual Machine does not experience internal memory fragmentation or erratic structural inflation, no matter how huge data structures that the user might have are (Cooper and Torczon, 2011). 

##### **3.8.2 Execution State and Scope Isolation (Activation Records)** 

Although by means of the elegant solution of the polymorphic data storage problem in a homogeneous geometrical form the type-tagged abstraction provides a clear solution, it also presents an immediate architectural problem itself. How does the Virtual Machine handle the state of execution and avoid localized data cross-contamination on a jumping-between-the-highlyisolated scope of functions? 

##### **_Virtualized Memory Spaces_** 

In more conventional compiled languages subroutine execution and local variable storage is simply delegated to the underlying host operating system physical stack. Although this has high performance, it lets go of architectural control. When a program gets into an infinite recursion state, the physical hardware stack will eventually overflow and lead to a fatal, uncatchable crash handled by the operating system instead of the compiler (Smith & Nair, 2005). MOON architecture overtly drops the use of physical call stack of the host to ensure stability and determinism. The Virtual Machine instead implements its own addresses, contiguous software memory space, which is a 

32 

Last-In-First-Out (LIFO) type. The engine acquires final, absolute control over its memory footprint by being able to virtualize the execution stack. It is capable of mathematically observing its realtime spatial usage and the architecture can softly handle deep-recursion thresholds and produce safe, diagnostic runtime exceptions instead of experiencing catastrophic system failures. 

##### **_Activation Records_** 

The execution of a subroutine by a standard program needs to have a separate workspace where the local variables are kept. Assigning each individual function call a brand new, independent memory array would impose tremendous computational load, and would cause memory fragmentation to be continuous. The MOON architecture completely avoids the process of allocation and instead uses a theoretical construct called an Activation Record (often called a Call Frame in systems literature). The implementation engine only has a single huge, global stack of memory. After a function is called, the Virtual Machine creates an Activation Record which is a very localized, lightweight metadata data structure that conceptually covers up the global stack. Such a record forms a sliding window. It determines a rigid geometric bottom boundary on the world stack at which the parameters of the new function and local variables (in the new function) start. This base boundary, to the active subroutine, is the absolute zero. Although the variables of the function may be in practice at the position of the 500th slot in the global stack, the Activation Record can transform that position mathematically to make it appear to the function as slot 0. This is a beautiful topological masking that structurally makes sure that the scope is independent, mathematically impossible that a child function can accidentally read or corrupt the local memory of the parent function that called it, and does this without ever allocating a new memory (Nystrom, 2021; Scott, 2015). 

##### **_State Preservation and Execution Unwinding_** 

On top of the spatial nature of isolation of Activation Records, there is the vital temporal function. A running program is a sequential chronological process. When the execution engine is presented with a call to a function it has to put its current chronological timeline on hold and instead, jump to a completely new sequence of logic, execute that logic and then resume the original timeline perfectly seamlessly. The architectural mechanism which follows this temporal history is the Activation Record. The currently active record captures the geometric position of active sequence pointer at the exact spot before the Virtual Machine allows execution to cross the boundary into a new localized environment. It basically marks the very spot of micro-operation where execution was suspended. 

When a subroutine comes to completion (a return or give command is received), the Virtual Machine begins execution unwinding. It will kill the active top-level Activation Record and will move the memory window back down to the base of the parent, immediately destroying all the localized variables that have been generated in the subroutine. Lastly, it recovers the sequence pointer of the resurrected parent record as a cache. The engine is then restarted at the same bookmark in the same fetch-decode-execute cycle and an unbroken chain of execution logic between nested function calls is upheld (Cooper and Torczon, 2011). 

##### **3.8.3 The Fetch-Decode-Execute Cycle and Polymorphic Dispatch** 

With the underlying memory model safely encircled under Type-Tagged Abstractions (Section 3.8.1) and the execution states safely encircled under the Activation Records (Section 3.8.2), all that is required of the Virtual Machine then is an algorithmic engine to proactively enforce the compiled logic. An execution engine, in the theory of computer science, is a deterministic automaton, which, given the instructions presented to it in a linear order, switches between them and updates the overall state of the program. 

33 

##### **_The Emulated Dispatch Cycle_** 

The emulated dispatch cycle is the basic architectural structure of the MOON Virtual Machine, the operating engine pulse that is always constant and hypothetical. This can be explained by the fact that the Virtual Machine is created to replicate a real-life microprocessor, and it is an emulation of the conventional Von Neumann architecture by using software (Patterson and Hennessy, 2013). This is a three step, three micro-algorithmic step implementation process: 

1. Fetch: The engine reads the following single-byte command of the active compiled code and that command is also atomic. 

2. Decode: The engine interprets the numerical value of this byte and uses it as a specific and fixed operation instruction (e.g. mathematical addition, loading a memory address or logical comparison). 

3. Execute: The engine carries out topological or mathematical manipulations on the localized variables and the primary execution stack and then changes the state to jump to the next cycle (Smith & Nair, 2005). 

##### **_Topological Control Flow and Sequential State Tracking._** 

The complex trail over the algorithmic routing is one such key fact of this dispatch cycle. The semantic constructs that are used by developers to specify the flow of control in the original humanreadable source code include if, else, and while. However, all these notions are unfamiliar to the Virtual Machine. Phase 3 involved converting all the semantic flow of control to a form entirely of geometric mathematics by the compiler. The Virtual Machine to navigate this constructed logic, the Virtual Machine has a logical base of its internal state; a marker which is used to point to the exact time point to which the running sequence is at. 

When the dispatch cycle is to evaluate a conditional branch, it simply removes a Boolean value off the execution stack. When the value is found to be false, the engine does not run the code, it reads the code and disregards it. Instead, the current teaching provides a fixed numerical offset. The Virtual Machine directly takes this offset on the Instruction Pointer and immediately shifts its spatial location in the sequence array. The architecture supports O (1) branching speed of algorithms by completing all the branching and looping with solely spatial addition and subtraction. Theoretically, a relatively few computational steps are required to skip a huge ten-thousand-line block of conditional code that is mathematically equivalent to executing one operation. 

##### **_Late-Bound Polymorphic Operations_** 

Due to MOON being dynamically typed, there is no assurance of the data type of any variable that the compiler frontend can know at the stage of the Bytecode Generation. This means that it is unable to issue instructions that are type specific (e.g., give different instructions to add integers different instructions to add floating-point numbers different instructions to concatenate strings). The compiler creates one universal mathematical instruction. This architectural constraint compels the Virtual Machine to make use of Late-Bound Polymorphism, and semantic validation and typechecking is deferred to the very final microsecond of execution (Scott, 2015). 

The conceptual model that fits best in this mechanism is addition operation. Upon receiving an addition directive on the dispatch loop, the dispatch loop removes two operands off the execution stack. Any mathematics command is performed by dynamically questioning the Type Tags that are stored in the memory envelopes (as defined in 3.8.1). The addition instruction, just by this real-time metadata, is autonomously mutated to change its inside algorithmic behavior: 

- Strict Arithmetic: In the case that both tags represent numbers, the envelopes are sent to the Arithmetic Logic Unit (ALU) simulator, where pure mathematical addition is done. 

- Structural Coercion: In case one of the tags is a character string the engine will intercept the math. It independently forces the opposite operand to a string representation and does spatial memory concatenation to a new, unitary text structure. 

34 

- Topological Merging: When the leading tag denotes a dynamically sized List, the engine will instigate a profound structural copy, which will proceed through the secondary operand and add its elements until the creation of a brand new, blended sequential amass. 

- Safe Degradation: In the case of the tags being completely incompatible (e.g. trying to add a Boolean to a dictionary), the operation forcibly refuses the evaluation, unwinds the active Activation Records and completes the system safely with a diagnostic exception. 

The Virtual Machine makes certain that all is absolutely type-safe, yet the size of the overall Instruction Set Architecture is kept as low as possible, by concentrating the polymorphism within the dispatch cycle. One conceptual instruction can effectively substitute tens of very specialized, bloated opcodes. 

#### **3.9 Architectural Design of Core Data Structures** 

Even though the execution stack of the Virtual Machine is highly optimized to run immediate, limited primitives (e.g. Booleans and discrete numbers), the real power of the MOON language can be attributed to its capability to manipulate unlimited, complex data structures. The phase 4 system memory models it can no longer store such structures as dynamic lists and entity dictionaries directly in the execution stack since they exhibit a geometric expansion that can grow at any time. These values are stored as values referred to in an extraneous dynamic memory pool. These underlying data structure algorithms are highly restrictive of the computational efficiency of the whole MOON programming language, specifically its ability to merge sets of objects in a brief period of time, run through arrays, and respond to object properties. In case the theory behind the underlying memory models is unsound, the Virtual Machine will suffer catastrophic temporal latency and deplorable memory fragmentation (Skiena, 2008). The MOON architecture implements two canonical, highly optimized data structures, the Dynamic Array (sequence lists) and the Hash Map (dictionaries and state tracking at a global scale) to make sure that it provides elite computational performance. 

##### **3.9.1 Dynamic Arrays (Lists)** 

The MOON language users can access sequence data in normal bracket notation (e.g. let sequence be [1, 2, 3]). This can appear as to the user a very huge expandable list of extreme length, but it has a simple hardware-level architectural contradiction. 

##### **_The Contiguity Memory Constraint._** 

Array architectures need strict spatial contiguity in order to be powered optimally. Physically, all the elements of a list are supposed to be in immediate contact with the final element of the physical memory of the host. This nearness is mathematically untenable because of two reasons. First, it ensures spatial locality, and this allows the host microprocessor to store the array in a cache which makes sequential loops (like in the for each constructs of MOON) to run quickly. Second, it guarantees that accessing an element by index (e.g., sequence[5]) is always O(1)-time, since calculation is only done by computing a fixed mathematical offset of the base of memory address of the array (Cormen, Leiserson, Rivest, and Stein, 2009). Fixed in position, however, are pure contiguous arrays; and their geographical limits are fixed as soon as they exist. In the case the user attempts to add a fourth object to a three-object fixed array, the neighboring memory may already be used up with unrelated program data and therefore there will be an overflow collision. 

##### **_The Doubling Algorithm with Amortization_** 

To eliminate the paradox of having a structure which must be strictly contiguous, and at the same time be infinitely expandable, the MOON architecture models are revealed as Dynamic Arrays. This building is also not tied to its geographical footprint because it does not tie its logical identity to its physical allocation. The architecture has an internal structure that is based on two different integers of the instantiated lists: 

35 

1. Count (Logical Size): This is the count of active elements of semantics present within the sequence. 

2. Capacity (Physical Limit): It is the physical limit on the memory block which is being currently allocated to the structure. 

The initial capacity of the MOON list is initialized at a small value (i.e., 8 slots) at the time of initial creation. The number of items multiplies with the number of items that the user adds to the list. As the capacity is artificially bigger than the count, these insertions do not require structural mutation, and require O(1) time. This design of the architecture leads it to the point where the number of values is equal to its capacity. Virtual Machine interrupts the instruction when the user attempts to insert an element in a full array. Naturally, it is mathematically applied to the product of the current capacity and some fixed growth factor, usually doubling (2x) the current capacity. Then the engine requests the outside dynamic pool to allocate to it a new, and considerably larger, adjacent block of memory. It operates by using a fast spatial copy to copy every element accessible in the old memory block to the new and larger envelope and methodically destroys the old block in order to prevent memory leaks. The new element is then safely placed in the new space, which is available. 

##### **_Mathematical Time Complexity of Operations_** 

The cost of a reallocation event is high as it requires a O(N) linear time to duplicate each of the elements sequentially. As long as the architecture did not merely insert a slot in the array with each new element, list-wide reallocations would make the total insertion time catastrophic, with quadratic complexity of O(N 2 ) (Skiena, 2008). However, the architecture may employ the approach of geometric doubling to make sure that the reallocation events increase exponentially as the length of the list increases. The cost of the reallocations has been mathematically spread out over the millions of operations of instantaneous insertions when a list has a million elements. Thus, the time of adding a new element to a MOON list is amortized constant (1). It is this better topological scaling that allows the Virtual Machine of MOON to execute powerful operations, such as scaling a mathematical domain (1 to 10000) into a physical list, deep-cloning and merging two arrays with the polymorphic ADD opcode, etc., to be blindingly run with smooth efficiency. 

##### **3.9.2 Hash Maps (Dictionaries and System Registries)** 

Dynamic Arrays are faulty at dealing with associative data, although they are effective with dealing with the problem of storing sequences of data. An architecture built on a linear array to match semantic keys with specific values (e.g. the word health with the integer 100) would require a linear scan of the entire array to find the specific value. This operation is disastrous to execution time since, as the size of dataset increases, then this operation is an O(N) operation, and this is catastrophic. To enable O(1) associative lookups, which are mathematically guaranteed, the MOON architecture uses a solid Hash Map (Dictionary) architecture. Not only is this data structure made available to the user in its exposed form, as a syntax feature, but it is the real architectural infrastructure of the Virtual Machine, such as global variable monitoring and the phrasal functionality register of the parser. 

##### **_Hashing_** 

The space of the memory of a specific execution engine is, in essence, only addressed using integers. Therefore, to place a random variable-length string (say, the key of a dictionary, or the name of a global variable) in a fixed geometric memory address, the string will require mathematical coding into a numerical index. To achieve this the FNV-1a (Fowler-Noll-Vo) hashing algorithm was utilized in the MOON architecture. FNV-1a is a simplified version of cryptographic algorithms that are designed to provide functionality, speed, and structural distribution (Cormen, Leiserson, Rivest, and Stein, 2009). The algorithm operates in sequence, character by character and sequentially performs a bit-wise XOR operation on binary-value of the character and multiplies it by a mathematically pure prime number. 

36 

This is the order that guarantees a property which is of paramount significance to computer science that is called the avalanche effect. One change in a microscope to just one of the strings, such as substituting good with goods, generates a 32-bit integer which is entirely different. This mathematical dispersion makes sure that the keys are randomly spread in the memory space and hence minimal clustering occurs. 

##### **_Index Bounds and The Pigeonhole Principle_** 

Once the 32-bit hash computation is done, the architecture must then limit this enormous number to the actual geometric capacity of the allocated memory array. Modulo arithmetic is used to ensure that the hash has a size that is equal to the current size of the physical table (e.g. a 105-value hash wrap-wrapped in a table of size 105 would be reduced mathematically to an index of 1). Nonetheless, reducing the potential number of combinations of strings to an infinitely large memory array is sure to trigger the Pigeonhole Principle. The mathematical certainty is that we shall come to a point when two strikingly dissimilar strings shall resolve to the same index. 

##### **_Linear Probing Collision Resolution_** 

When the architecture attempts to insert a new key in an index that is already in use by another key, the architecture will cause a collision. To rectify this, MOON makes use of the Open Addressing collision resolution algorithm referred to as Linear Probing (Knuth, 1998). Linear Probing does not form a second linked list upon the occurrence of a collision. It rather skips to the next memory address (index +1). Where even that slot is occupied, it will queue up until an empty spatial boundary is located, and place the payload in it. Linear probing is highly recommended over traditional linked-list chaining as an architectural design in microprocessor optimization. Linear Probing has the same hard spatial memory contiguity, as in this technique of collision resolution, neighboring memory slots are searched. This allows the host CPU to aggressively access its L1 data cache which removes collisions and has a hardware latency of close to zero (Bryant and O'Hallaron, 2015). 

##### **_Load Factors and Dynamic Resizing._** 

Linear Probing naturally causes so called clustering when a hash map is filled up with information: long, contiguous chain of occupied memory slots. When a table becomes full enough to an extent this engine must scan massive groups to find an empty slot or a key, and this is disastrously reducing the lookup time, which is now O(N) rather than O(1). To prevent such degradation, MOON architecture takes a Load Factor, which is the ratio of active element to total physical capacity. The Virtual Machine is limited very tightly on the maximum load factor (theoretically to 75 percent). Once the number of elements stored in a hash map exceeds this 75% threshold, then the architecture notices the operation and triggers an expansion. Similar to the Dynamic Array protocol, the engine allocates twice the memory space. It is, however, not merely able to put the old things into the new space linearly. The modulo boundaries change as a result of the doubled increase in the geometric capacity of the table. The architecture must be able to re-work every single element in the existing table systematically in the sense of re-hashing them and placing them into completely new and mathematically un-clustered locations in the new extended memory envelope. Such reallocation is transient with an O(N) cost but the factor of exponential growth makes amortization of insertion times be O(1)-guaranteed (Skiena, 2008). 

##### **_System-Wide Architectural Integration_** 

As this realization of the HashMap is not just extremely robust but also consumes less memory, it was adopted as the universal associative architecture of the MOON ecosystem, three key applications of which are: 

37 

1. User Data Structures: It is the basis of the MOON Dictionary syntax (e.g., { key: value }), and it offers programmers the capability of forming an elaborate, O(1) runtime expression of sophisticated, JSON-like objects. 

2. Global State Resolution: Virtual Machine has a master Hash Map so as to facilitate the globally scoped variables, as discussed in Section 3.7.3. The VM loads a memory payload of this global table in constant time when the compiler produces a GETGLOBAL opcode, when the target string is being hashed. 

3. The Phrasal Signature Registry: The compiler operates under the frontend (pre-pass stage of 3.6.5) by comparing the natural language function calls using a HashMap. The naming of a function (e.g., sum) is hashed, and serves as the dictionary word, which matches a direct match with the root of the Deterministic Finite Automaton (Trie) data structure. This provides that there is no overhead of a multi-word phrasal function of high complexity, which would be zero-lookup. 

#### **3.10 The Native Method Interface (The Bridge)** 

By definition, a Virtual machine is a fully closed and solipsistic system. The execution engine reads and writes the instructions in a written form and manipulates an internal memory of data by accessing and manipulating its own simulated memory. It is, nevertheless, a simulated world where people have no conceptual knowledge of the real-life universe outside of it. The Virtual Reader does not know that there is a computer monitor; he does not know about the existence of a hard drive, and he does not know of the flow of time. Moreover, without making the MOON program language restricted to this execution loop, it could only be in theory, a calculator. This architecture should have an escape hatch called a Foreign Function Interface (FFI) that can be conceptually described in this architecture as Native Method Interface (Scott, 2015). 

##### **_Architectural Sandbox Dilemma and Host-Language Emancipation_** 

The fundamental structural distance between the emulated dynamic environment of MOON and the fixed pre-compiled physical nature of the operating system on which it is executed is the Native Method Interface. In this specific methodology the C programming language, the host environment has direct, unlimited access to hardware in the machine since MOON Virtual Machine is essentially implemented on top of a host language. C can ask the operating system clock, physically allocate memory and deal with I/O interrupts to display characters to a physical display. To access these bare C functions to the interpreted MOON code, the Native Bridge has been designed to provide safe access. An intrinsic subroutine to the human developer writing in MOON appears just like any other user-written MOON subroutine. They refer to it using the standard syntax (e.g. ‘show 2 * 3’ or ‘clock’). However, internally, there is a different design of architectural routing. 

##### **_The Temporal Benefit of Native Implementation_** 

One of the key reasons of routing operations through the Native Bridge is the speed of raw execution. Whenever an average MOON operation is executed, then the Virtual Machine must jump through the fetch-decode-run loop and execute one step at a time. This emulation has an implicit cost of latency. Conversely, invocation of a Native Method by the architecture does not invoke at all the dispatch loop of the Virtual Machine. The execution engine halts its interpretation of the byte code and transfers the control of the execution to the compiled C code. As the C function is in raw and highly optimized machine code that is directly able to communicate with the microprocessor, the code is run at the highest possible speed that is possible by the actual hardware. Offloading computationally expensive operations, or I/O operations to such native bindings is such that the MOON language achieves virtually native performance numbers in the areas where it counts (Smith and Nair, 2005). 

38 

##### **_The Implementation Strategy: Interception and Translation_** 

The structural translation protocol required between a dynamically typed emulated stack and a statically typed compiled host language is very fine. This cannot be accomplished by the implementation engine simply by casting a MOON "Type-Tagged Memory Envelope" around a C function since even the C compiler has no intrinsic concept of the internal data format of the Virtual Machine. The Native Bridge must also be a bi-directional translator. This architectural translation occurs in four mathematical that are sequenced: 

1. Interception and Arity Validation: When a dispatch loop is being executed, on reaching an identified function call, the Type Tag of the target is checked. When reported in the tag as a Native Function, execution of a bytecode by the engine is stopped. It then requests the interface to determine what its arity is (that is to say the number of arguments required by the underlying C function). 

2. Downward Translation (Unboxing): The Virtual Machine works back down the execution stack trying to find the arguments that it has been supplied. The bridge must then unbox these arguments in a methodical manner. It extracts the dynamic metadata labels of the Virtual Machine, and the real underlying code (i.e. drawing a raw 64-bit float out of a MOON Number object), and pushes such primitives into an array form that may then be manipulated directly with the inflexible C function. 

3. Host Execution: Once the arguments have been converted into native hardware types, the Virtual machine makes the C function pointer call. The host language itself executes, running its high-speed logic and interacts with the operating system and generates some raw output value. 

4. Upward Translation (Re-encapsulation): Once the C code has completed processing it provides a primitive value in an untidy form. This uncoded information is then hijacked by the bridge and the translation is then reversed back. It generates a new MOON Type-Tagged Envelope, and pumps the raw C result into the enclosed envelope, and simply blows this new object to the literal top of the Virtual Machine execution stack. Finally, the bridge resumes the deciphering of the bytecode by the dispatch loop. 

##### **_System-Wide Integration through the Global Registry_** 

To communicate this elaborate translation architecture clearly to the developer, the methodology makes use of the Global Hash Map that was designed in Section 3.9.2. This is done with the independent injection of specific string keys into the global registry relating them to these Native Bridges, by the compiler as the absolute first boot sequence of the Virtual Machine, and even prior to the user script itself being read. One such one is hashing the string clock and binds the mathematically to the C-native time function. Consequently, this triggers a case whereby calling the clock variable in the script of the user, the Virtual Machine attempts to look up a standard hash map and access the Native Interface and makes the translation bridge automatically be activated. The language MOON in this beautiful architectural illusion has an infinite prolongation. This hostlanguage bridging, memory translation and hardware interfacing complexity is completely transparent to the end user, so as to preserve the flowing, domain-specific beauty of the syntax of the language, but silently to connect to the very raw, brash speed of raw C (Louden and Lambert, 2011). 

#### **3.11 Memory Management and Garbage Collection** 

##### **3.11.1 The Allocation Problem and Dynamic Lifespans** 

In compiled systems-level languages, the systems are very deterministic in memory management and are manually managed by the programmer. The host operating system expressly requests the programmer to assign it memory and it is his or her job to deallocate it when the program no longer requires it. MOON is intended, however, to be a high level dynamically typed language to make it more cognitively fluid. It would be completely catastrophic to the ambitions of the language that a MOON developer is still manually allocating and freeing memory. As such, the Virtual Machine 

39 

will be obliged to automatically manage the physical memory footprint of the running script. Though primitive values are safe within the geometric boundaries of the execution stack, as provided in Section 3.8.1, complex data structures (dynamic lists, hash maps and interpolated strings) are also unlimited. In order to execute these structures, the Virtual machine must continually request dynamic heap variable-sized segments of memory in the host operating system (Jones, Hosking, and Moss, 2011). 

This poses an allocation problem. The physical memory available to the execution engine will soon be filled up, leading it to give a poor performance because the Virtual Machine will generate the objects referred to in the current running program (e.g. dynamically concatenation of strings in an excessive for-loop) when it needs. The Virtual machine is subject to out-of-memory (OOM) system crash which can be very catastrophic unless there is in place a system to isolate and remove all the idle information. Therefore, this architecture necessitates the integration of one component to restore the idle memory, a Garbage Collector. 

##### **3.11.2 Object Tracking and the Unified Memory Graph** 

Complete knowledge is critical to automatic memory recovery. Execution engine that is incapable of hypothetically determining the existence of an object cannot safely destroy an object. Unless a Virtual Machine dynamically allocates a string and records this fact in its internal registry, the memory is lost permanently, and this results in an irretrievable memory leak. MOON Virtual machine uses one tracking system in order to allow the perfection of account tracking of architectures. Regardless of the semantic identity of a particular entity, whether it is a user-defined list, dynamically-created closure or a native function binding, the notion of the architecture will establish all dynamically created data as a single, underlying abstraction of an Object. Any complex structure receives a standardized metadata header of the basic systemic data, including its type identity, and a flag of tracking (Scott, 2015). 

_Figure 4: The single global tracking list of all created objects_ 

More to the point, this simple metadata scheme inculcates a geometrical relation. When the Virtual machine requests the heap memory to create a new object, it transparently adds this new, minted object to a global intrusive tracking chain (theoretically a linked list of all created objects, _Figure 4_ ). This tracking chain will securely be pegged on absolute root state of the Virtual machine. Such an architectural design is how the Virtual Machine replicates the totality, of all the assigned memory into a traversable list structure. The user script of a user can thoroughly lose the localized variable reference to a list, but the Virtual Machine still has an absolute, topological anchorage to that block of memory in the global tracking chain, so that eventually it can be evaluated to be collected. 

##### **3.11.3 The Mark-and-Sweep Algorithm** 

To calculate the time within which an object is safe to be destroyed, the MOON architecture uses a classical tracing garbage collection algorithm, the Mark-and-Sweep (McCarthy, 1960). Unlike counting-references algorithms, which can be challenged by circular dependencies of memory (e.g. a list of entries, one of which contains a reference to that list), a tracing collector is an algorithm, which explores all the structure of the program under execution in order to determine reachability. The dynamically invoked collection cycle is a heuristics that depends on the pressure on the memory and is introduced in two distinct algorithm phases which are strictly separate: 

40 

##### **_Phase 1: The Trace and Mark Phase._** 

When made to invoke the garbage collector, the Virtual Machine will briefly interrupt the regular execution of the bytecode (a so-called "Stop-the-World" pause). This engine begins by the definition of a Root Set, set of memory pointers which by definition are active. The active execution stack values, all the variables in the active Activation Records (Call Frames) and the global registry of variables are in this Root Set. 



_Figure 5: A topological representation of reachability from the root set._ 

The algorithm systematically traverses this Root Set. Whenever it discovers an abstract reference to an object that is located on the heap, it locates it and updates its metadata tracking flag to reachable (we “mark” it). This traversal is also recursive. Upon the engine reporting that an object, like a Dictionary is accessible, then the algorithm would have to structurally descend into the internal arrays of the Dictionary, in turn marking all the keys and values stored in it reachable. This topological walkthrough is the calculation of the web of all the potential data the user might have access to using his/her script. 

##### **_Phase 2:The Sweep and Reclamation Phase._** 

Once the recursive traversal has found a limit of possible reachable paths the Sweep Phase is initiated by the execution engine. 

41 

















_Figure 6: The transition between the mark and sweep phases. Unmarked (and therefore unreachable) objects are identified and their memory is reclaimed, leaving only the marked, active memory graph._ 

The Virtual Machine rotates out of the execution stack of the user and traverses the absolute global tracking chain in a linear manner as shown in the transition phases of _Figure 6_ then constructed during the object allocation phase ( _Figure 4_ ). The engine queries the metadata tracking flag when it is executing all the individual objects previously allocated by the runtime. When the object contains the reachable flag, the structural proof is that it is being used by the program. The engine is safely turned off to then be ready with the next eventual collection cycle and the memory is left as such. 

Conversely, reaching an object with no reachable flag in the engine has been proven mathematically to imply that the object is unreachable: the script being interpreted by its user has no more routes to this information. It is garbage and needs to be reclaimed. The Virtual machine securely de-tethers the object of the global tracking chain and physically re-installs the block of memory to the host operating system. MOON Virtual Machine is designed so that such a two-phase memory synchronization can be provided by an infinite program, completely insulating the programmer of the architectural complexity of dynamic heap management and deterministic memory reclaim. 

#### **3.12 System Diagnostics and Fault Tolerance** 

The efficiency with which theoretical language architecture implements perfect logic cannot be used to judge the language but the true power of a language is determined by the ability to graciously cope with structural and operational failure. It is not a usable software tool in case an execution machine may directly cause a catastrophic crash of a host machine when it encounters a minor logical failure. The MOON architecture has a bilateral diagnostic framework that guarantees full systemic stability and provides a robust Developer Experience (DX), which is implemented during the compile-time analysis phase, as well as the runtime synthesis phase. 

##### **3.12.1 Compile-Time Resilience (Structural Synchronization)** **_The Fragility of Context-Free Grammars_** 

In Phase 2 (Syntax Analysis) the Pratt parser was expected to produce a mathematically sound Abstract Syntax Tree (AST). As the tree was created on the rigid and definite laws of a Context- 

42 

Free Grammar it is quite fragile by nature. Any unexpected symbol, such as an orphaned parenthesis, or a misspelled keyword, are radically opposed to the demonstrations of the active grammar rule. Traditionally, there is no forward mathematical path of a normal parser with an anomalous token. It must essentially cause a fatal syntax error and stop the compilation pipeline right away and make the programmer fix errors one at a time across numerous compilations (Aho, Lam, Sethi, and Ullman, 2006). 

##### **_Algorithmic Panic Mode and State Recovery_** 

In order to eliminate this bottleneck and realize the highest possible diagnostic throughput, the MOON frontend mixes a diagnostic error-handling tool known as Panic Mode. The grammatical error is not spotted directly, but when the parser identifies the error, the compiler is automatically terminated. Instead, it brings about a diagnostic error to the user and goes voluntarily into a local failure state (the "panic" state). The abstract interpretation of the parsing conceptualization of this state does not produce the Abstract Syntax Tree and is aware of the mathematical unsoundness and structural fragility of the existing geometric branch. The parser however, does not stop the reading of the source file but continues to consume the tokens of the scanner algorithmically. 

##### **_Boundary Re-alignment_** 

This forward-scanning step is geared towards removing the erroneous source code in the parser. The parser searches a structural definition, which is undeniable and unambiguous, also referred to as a _synchronization point_ . These limits are defined as hard statement endings (newlines) or as important control-flow words or statement starters (if, while, let, for) in MOON. After one of these known anchors has been swallowed by the parser, it finds itself aware that it has jumped so as to escape the corrupted expression and come into a whole new, grammatically different statement. The parser mathematically replenishes its panic state, and rewrites its grammatical environment and once again constructs a standard AST. This architectural strength permits the compiler to detect, separate, and communicate many and autonomous syntax errors in a single analytical execution, which has a significant effect on the life cycle of debugging. 

##### **3.12.2 Dynamic Fault Interception (Execution Graph Unwinding)** **_The Certainty of Runtime Mishaps_** 

Even though Panic Mode is a good method of eliminating grammatical anomalies in the source code, it does not protect against logical anomalies. As MOON is compiled by the Late-Bound Polymorphism and dynamic typing (written in Section 3.8), not all of its fatal violations can be mathematically detected during the compile-time. An example is that the compiler has no idea that a variable will dynamically become zero at some point before a division operation and that an object will possess the desired attribute at some point of execution. The anomalies will certainly manifest themselves as the runtime faults. 

##### **_Interception-Safe-Procedures_** 

Even in the case that the Virtual Machine attempted an attempt to divide by zero or a corrupt memory read into the microprocessor of the host operating system, the Virtual Machine would send a fatal hardware interrupt (equivalent to a Segmentation Fault) and crash the remainder of the host application. In order to escape this, a structural firewall that cannot be penetrated is the core dispatch cycle of the Virtual Memory. The dispatch engine examines the geometric boundaries and Type Tags of the operands in operation then carries out any volatile mathematical or topological operation. A mathematical violation that is identified by the engine is safely intercepted. The Virtual machine is initiated to prematurely terminate its simulated execution sequence before the illegitimate operation is transferred to the host hardware, thereby eradicating the chances of physical corruption of the memory completely. 

43 

##### **_Stack Tracing (Execution Unwinding)_** 

Though a fault interception is not the entire diagnostic procedure, it is only the initial half to inform the user of the failure of the Virtual Machine. In order to do so, the architecture initiates an algorithm process named Graph Unwinding of Execution. In case of the fault being intercepted, the Virtual machine capitalizes on the deep topological history which is recorded in its Activation Records (Call Frames which is in Section 3.8.2). The diagnostic engine executes backwards along the stack of the active Call Frames, and reads the caches of the Execution Pointers, at each level of extent. The failure in the unwinding of such temporal history enables the engine (nearly) to recreate the chronological sequence of the execution graph to the single microsecond of failure. The Virtual machine then converts this mathematical graph into a highly accurate, human-readable stack trace, which identifies the name of the specific file, the specific phrasal function, and the exact line number in that specific file where the anomaly occurred. Finally, the Virtual machine is used to arrange safely and controlled closure of the virtual environment and gracefully returns to the host operating system, after transmitting the diagnostic payload. 

#### **3.13 Conclusion** 

This chapter has fragmented in a systematic way, the architectural design, and theoretical approach to the implementation of the MOON programming language. The system has succeeded in repairing the gap between the very expressive, natural language pseudocode on the one hand, and deterministic machine execution on the other hand, by breaking the compilation pipeline down into very rigorous, mathematically sound stages. These phases of raw lexical tokenization, Top-Down Operator Precedence (Pratt) parsing, localized code generation, and lastly to the polymorphic dispatch cycle of the Virtual Machine are a complete, full-fledged language structure. Moreover, the amortized O(1) operation on data structures, automatic Mark-and-Sweep memory reclamation engine, and a bilateral fault-tolerant system - all these are combined to deliver the execution engine not only with high performance but structural stability. The architectural analysis is completed, and theory and conceptual blueprints of pure computer science is now solid. The background has been worked out towards the change of abstract modelling to physical software engineering. The following chapter will define how these theoretical systems were in fact put in practice and how the blueprint architectural models would be turned into high-performance and working C code. 

44 

## **REFERENCES** 

- Aho, A. V., Lam, M. S., Sethi, R., & Ullman, J. D. (2006). _Compilers: Principles, techniques, and tools_ (2nd ed.). Pearson Education. 

- Appel, A. W. (2002). _Modern compiler implementation in C._ Cambridge University Press. 

- Bendersky, E. (2009). _Some problems of recursive descent parsers_ . Retrieved from Eli Bendersky's Website. 

- Brooks, F. P. (1987). No silver bullet: Essence and accidents of software engineering. _Computer, 20_ (4), 10-19. 

- Bryant, R. E., & O'Hallaron, D. R. (2015). _Computer systems: A programmer's perspective_ (3rd ed.). Pearson. 

- Chomsky, N. (1956). Three models for the description of language. _IRE Transactions on Information Theory, 2_ (3), 113-124. 

- Cooper, K. D., & Torczon, L. (2011). _Engineering a compiler_ (2nd ed.). Morgan Kaufmann. 

- Cormen, T. H., Leiserson, C. E., Rivest, L. R., & Stein, C. (2009). _Introduction to algorithms_ (3rd ed.). MIT Press. 

- Crockford, D. (2007). Top down operator precedence. In A. Oram, & G. Wilson, _Beautiful code_ (pp. 143-156). O'Reilly Media. 

- Guzdial, M. (2015). _Learner-centered design of computing education: Research on computing for everyone._ Morgan & Claypool Publishers. 

- Hopcroft, J. E., Motwani, R., & Ullman, J. D. (2006). _Introduction to automata theory, languages, and computation_ (3rd ed.). Pearson. 

- Ierusalimschy, R., de Figueiredo, L. H., & Celes, W. (2005). The implementation of Lua 5.0. _Journal of Universal Computer Science, 11_ (7), 1159-1176. 

- Jones, R., Hosking, A., & Moss, E. (2011). _The garbage collection handbook: The art of automatic memory management._ CRC Press. 

- Knuth, D. E. (1998). _The art of computer programming, volume 3: Sorting and searching_ (2nd ed., Vol. 3). Addison-Wesley. 

- Koulouri, T., Lauria, S., & Macredie, R. D. (2014). Teaching introductory programming: A quantitative evaluation of different approaches. _ACM Transactions on Computing Education (TOCE), 14_ (4), Article 26. 

- Lindholm, T., Yellin, F., Bracha, G., & Buckley, A. (2014). _The Java virtual machine specification._ Addison-Wesley Professional. 

- Louden, K. C., & Lambert, K. A. (2011). _Compiler construction: Principles and practice._ Cengage Learning. 

- Luxton-Reilly, A., Simon, A. I., Becker, B. A., Giannakos, M., Kumar, A. N., Ott, L., . . . Szabo, C. (2018). Introductory programming: A systematic literature review. _Proceedings of the 23rd Annual ACM Conference on Innovation and Technology in Computer Science Education (ITiCSE)_ , (pp. 55-106). 

- MacLennan, B. J. (1999). _Principles of programming languages: Design, evaluation, and implementation_ (3rd ed.). Oxford University Press. 

- McCarthy, J. (1960). Recursive functions of symbolic expressions and their computation by machine, Part I. _Communications of the ACM, 3_ (4), 184-195. 

- Nystrom, R. (2021). _Crafting interpreters._ Genever Benning. 

- Parr, T. (2010). _Language implementation patterns: Create your own domain-specific and general programming languages._ Pragmatic Bookshelf. 

- Patterson, D. A., & Hennessy, J. L. (2013). _Computer organization and design: The hardware/software interface_ (5th ed.). Morgan Kaufmann. 

- Pratt, V. R. (1973). Top down operator precedence. _Proceedings of the 1st Annual ACM SIGACTSIGPLAN Symposium on Principles of Programming Languages (POPL '73)_ , (pp. 41-51). 

- Resnick, M., Maloney, J., Monroy-Hernández, A., Rusk, N., Eastmond, E., Brennan, K., & Kafai, Y. (2009). Scratch: Programming for all. _Communications of the ACM, 52_ (11), 60-67. 

45 

Robins, A., Rountree, J., & Rountree, N. (2003). Learning and teaching programming: A review and discussion. _Computer Science Education, 13_ (2), 137-172. 

- Scott, M. L. (2015). _Programming Language Pragmatics_ (4th ed.). Morgan Kaufman. 

- Sebesta, R. W. (2012). _Concepts of programming languages_ (10th ed.). Pearson. 

- Shi, Y., Casey, K., Ertl, M. A., & Gregg, D. (2008). Virtual machine showdown: Stack versus registers. _ACM Transactions on Architecture and Code Optimization (TACO), 4(4)_ , pp. 1-36. 

- Skiena, S. S. (2008). _The algorithm design manual_ (2nd ed.). Springer. 

- Smith, J. E., & Nair, R. (2005). _Virtual machines: Versatile platforms for systems and processes._ Morgan Kaufmann. 

- Stefik, A., & Siebert, S. (2013). An empirical investigation into programming language syntax. _ACM Transactions on Computing Education (TOCE), 13(4)_ . 

- Sweller, J. (1988). Cognitve load during problem solving: Effects on learning. _Cognitive Science_ , pp. 257-285. 

- Weintrop, D., & Wilensky, U. (2015). To block or not to block, that is the question: Students' perceptions of blocks-based programming. _Proceedings of the 14th International Conference on Interaction Design and Children_ , (pp. 199-208). 

- Wilson, P. R., Johnstone, M. S., Neely, M., & Boles, D. (1995). Dynamic storage allocatoin: A survey and critical review. _International Workshop on Memory Management_ , pp. 1-116. 

- Wirth, N. (1996). _Compiler Construction._ Addison-Wesley. 

46 

