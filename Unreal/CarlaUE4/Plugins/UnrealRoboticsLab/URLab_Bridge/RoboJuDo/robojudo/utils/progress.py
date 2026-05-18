from tqdm import tqdm


class ProgressBar:
    def __init__(self, tag, total):
        self.tag = tag
        self.total = total
        self.pbar = tqdm(
            total=total,
            desc=tag,
            unit="step",
            colour="magenta",
            ncols=100,
            dynamic_ncols=True,
            mininterval=0.01,
            leave=False,
            ascii=True,
        )

    def update(self, step=1):
        self.pbar.update(step)

    def set(self, n):
        if self.total > 0:
            n = max(0, min(n, self.pbar.total))  # clamp
        n = round(n, 3)
        delta = n - self.pbar.n
        self.pbar.update(delta)

    def close(self):
        self.pbar.close()
        # print("")


if __name__ == "__main__":
    import time

    pbar = ProgressBar("Test", 100)
    pbar2 = ProgressBar("Test alpha", 1)
    for i in range(1, 101):
        pbar.update(1)
        pbar2.set(i / 100)
        time.sleep(0.01)
    pbar.close()
    pbar2.close()
