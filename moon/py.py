with open("stress.moon", "w") as f:
    f.write("let massive_list be [" + ", ".join(f"{i+1}" for i in range(10000)) + "]\n")
    f.write("show \"List size is: `massive_list\'s length`\"\n\n")
    f.write("let counter be 0\n")
    f.write("if true:\n")
    for _ in range(300):
        f.write("  add 1 to counter\n")
    f.write("end\n")
    f.write("show \"Block executed. Counter is: `counter`\"\n")
