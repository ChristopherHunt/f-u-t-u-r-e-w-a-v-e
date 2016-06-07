#! /usr/bin/env python3

import argparse
import os
import sys

import matplotlib
from matplotlib import pyplot

import numpy
import pandas

from collections import Counter

import locale
locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')

gold='#FADA5E'
green='#0A7951'
green_gold ='#82AA58'
color_vals = ['purple', 'orange', 'red', green, 'blue', 'black', 'magenta', 'red']
color_vals = ['purple', 'red', 'green', 'yellow',]
marker_vals = ['o','v','p','H','D','s','8','<','>']
marker_vals = ['o','v','s','D']
params = {'text.usetex' : True,
          'font.size' : 11,
          'font.family' : 'lmodern',
          'text.latex.unicode': True,
          'legend.loc' : 'best',
          'lines.color' : green
          }
matplotlib.rcParams.update(params);
default_color = green

description = 'A plotter for CPE-564 Spring 2016'
def get_arg_parser(description):
    return argparse.ArgumentParser(
            prog=sys.argv[0], description=description)

def parse_csv(file):
    new_csv = None
    new_csv = pandas.read_csv(file)
    return new_csv


def main():
    arg_parser = get_arg_parser(description)
    arg_parser.add_argument('-f','--filename', metavar='<filename>', help='CSV file with datas')
    args = arg_parser.parse_args()
    csv_data = None
    with open(args.filename, 'r') as file:
        csv_data = parse_csv(file)
    assert(csv_data)
    return 0

if __name__ == '__main__':
    rtn = main()
    sys.exit(rtn)
