Hey Copilot, I have a GENIUS idea for you. Instead of making an entire flux compiler, in flux, just use the one we already have made, and compile that into the kernel that we already compile anyways. We can just compile the flux compiler to operate freestanding, which can then be used to run flux code in JIT and AOT as-is.



Wait, I see you mention port it to compile freestanding. I surely hope you don't mean re-write everything to not use STL. Because something you can do is to just also compile all STL to pack it into the executable.