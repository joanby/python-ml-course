import unittest
from os.path import dirname

def main():
    tr = unittest.TextTestRunner(verbosity = 2)
    suite = unittest.defaultTestLoader.discover(dirname(__file__),
                                                pattern='test*')
    tr.run(suite)

if __name__ == '__main__':
    main()
