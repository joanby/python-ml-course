
def default_key(x):
    """ Default comparison function """
    return x

def order(seq, key = default_key, reverse = False):
    """ Return the order in which to take the items to obtained
    a sorted sequence."""
    o = list(range(len(seq)))

    def wrap_key(x):
        x = seq[x]
        return key(x)
        
    o.sort(key = wrap_key, reverse = reverse)

    return o


