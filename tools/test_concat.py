import time

def benchmark_concat(iterations):
    print("Starting Python string concatenation...")
    start_time = time.time()
    
    text = "moon"
    for i in range(iterations):
        text = "a" + text + "b"
        
    end_time = time.time()
    print(f"Final String Length: {len(text)} characters")
    print(f"Time Taken: {end_time - start_time:.5f} seconds")

benchmark_concat(100000)
