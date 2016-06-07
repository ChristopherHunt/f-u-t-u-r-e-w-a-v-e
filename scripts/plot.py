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

def parse_filename(filename):
    delay1 = 250
    delay2 = None
    num_syncs = None
    split = os.path.basename(filename).split('_')
    delay2 = int(split[0])
    num_syncs = int(split[1].replace('sync','').replace('.txt', ''))
    return (delay1, delay2, num_syncs)

def parse_csv(file):
    new_csv = None
    new_csv = pandas.read_csv(file, comment='#')
    return new_csv

def format_data(data):
    data.columns = ['time', 'max client delay']
    first_time = min(data['time'])
    for i in range(len(data['time'])):
        data['time'][i] = data['time'][i] - first_time
    print(data)
    return

def plot_data(data, filename='whatever.pdf'):
    title = 'Convergence for Client 1: {}ms Client 2: {}ms with {} Sync Messages'
    title = title.format(data.delay1, data.delay2, data.syncs)
    axes = data.plot(x='time', y='max client delay', kind='scatter', c=green, title=title)
    axes.set_xlim(left = -3000)
    pyplot.xlabel('Time (ms)')
    pyplot.ylabel('Max Client Delay (ms)')
    pyplot.savefig(filename, bbox_inches='tight');
    pyplot.close()
    return

def main():
    arg_parser = get_arg_parser(description)
    arg_parser.add_argument('-f','--filename', metavar='<filename>', help='CSV file with datas')
    args = arg_parser.parse_args()
    data = None
    delay1, delay2, syncs = parse_filename(args.filename)
    with open(args.filename, 'r') as file:
        data = parse_csv(file)
    format_data(data)
    figname = os.path.splitext(args.filename)[0] + '.pdf'
    data.delay1 = delay1
    data.delay2 = delay2
    data.syncs = syncs
    plot_data(data, figname)
    print(figname)
    return 0

if __name__ == '__main__':
    rtn = main()
    sys.exit(rtn)
