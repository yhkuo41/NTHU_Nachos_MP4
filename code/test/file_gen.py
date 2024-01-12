import math

def print_numbers(n):
    for i in range(1, n + 1):
        print("{:09d}".format(i)),
        if i % 10 == 0:
            print

kb = 1000
mb = kb * 1000
n = (((67.108864 * mb) * 10000 / (100 * kb)))  # 10000 is about 100K
print_numbers(int(math.floor(n)))