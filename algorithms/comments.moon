# ---------------------------------------------------------
# MULTILINE COMMENTS
# Showcases Moon's elegant, brace-free multiline comments
# that are driven purely by indentation.
# ---------------------------------------------------------

## This is a multiline comment!
   Notice how there are no closing tags like `*/`!
   As long as we indent past the start of the `##`,
     it is safely consumed by the compiler.
     
     Even empty lines are part of it!
   The compiler will ignore all of this completely.

let greeting be "Hello from Moon!"

# This is a standard single-line comment.
show greeting

## We can also use multiline comments to easily
   disable blocks of code without having to put
   a hash in front of every single line.
   
   let inactive (x):
     give x * 2
   end

show "You can see that the disabled code did not run."
