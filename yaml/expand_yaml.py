#!/usr/bin/env python  
import sys
import getopt

from pprint import pprint as pp  
from yaml import load,load_all,dump  

def usage( arg ):
    print "expand_yaml.py -f <inputfile> -d <device>"


def main(argv):
    try:
        opts, args = getopt.getopt(argv,"f:d:")
    except getopt.GetoptError:
        usage( 'h' )
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-d':
            d = str( arg )
            found_d = True
        elif opt == '-f':
            f = str( arg)
            found_f = True
        elif opt == '-h':
            usage( 'h' )
            sys.exit(2)

    if not ( found_d and found_f ): 
        usage( 'h' )
        sys.exit(2)

    hash=load(open(f))  
    pp(hash[d])  

if __name__=="__main__":
    sys.exit(main(sys.argv[1:]))
