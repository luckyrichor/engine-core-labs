"""Fully specified splitmix64 seed expansion and xoshiro256** integer stream."""
MASK = (1 << 64) - 1


def rotl(x, k):
    return ((x << k) | (x >> (64 - k))) & MASK


class Random:
    def __init__(self, seed):
        self.state = []
        for _ in range(4):
            seed = (seed + 0x9e3779b97f4a7c15) & MASK
            z = seed
            z = ((z ^ (z >> 30)) * 0xbf58476d1ce4e5b9) & MASK
            z = ((z ^ (z >> 27)) * 0x94d049bb133111eb) & MASK
            self.state.append(z ^ (z >> 31))

    def next(self):
        s = self.state
        result = (rotl((s[1] * 5) & MASK, 7) * 9) & MASK
        t = (s[1] << 17) & MASK
        s[2] ^= s[0]
        s[3] ^= s[1]
        s[1] ^= s[2]
        s[0] ^= s[3]
        s[2] ^= t
        s[3] = rotl(s[3], 45)
        return result

    def below(self, n):
        if not 0 < n <= 1 << 64:
            raise ValueError('random range must be in 1..2^64')
        limit = (1 << 64) - ((1 << 64) % n)
        while True:
            x = self.next()
            if x < limit:
                return x % n
