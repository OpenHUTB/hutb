import numpy as np


def almost_equal(item1, item2, r_thresh=0.01, a_thresh=1e-4):
    item1 = np.array(item1).flatten()
    item2 = np.array(item2).flatten()
    if len(item1) != len(item2):
        return False

    return all(np.isclose(item1, item2, rtol=r_thresh, atol=a_thresh))


def mean_square_err(im1, im2):
    """Compute the mean square error of the two images to determine if they
    are equivalent.

    Args:
        im1 (np array):
        im2 (np array):

    Returns: integer, lower is more similar

    """
    err = np.sum((im1.astype("float") - im2.astype("float")) ** 2)
    err /= float(im1.shape[0] * im1.shape[1])
    return err
