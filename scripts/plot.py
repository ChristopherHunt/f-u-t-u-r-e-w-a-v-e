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
    return data

def plot_data(data, filename='whatever.pdf'):
    title = 'Convergence for Client 1: {}ms Client 2: {}ms with {} Sync Messages'
    title = title.format(data.delay1, data.delay2, data.syncs)
    axes = data.plot(x='time', y='max client delay', kind='scatter', c=green, title=title)
    axes.set_xlim(left = -3000)
    pyplot.xlabel('Time (ms)')
    pyplot.ylabel('Max Client Delay (ms)')
    pyplot.savefig(data.figname, bbox_inches='tight');
    pyplot.close()
    return

color_vals = ['purple', 'orange', 'red', green, 'blue', 'black', 'magenta', 'red']
suffix = '.png'
def main():
    arg_parser = get_arg_parser(description)
    #arg_parser.add_argument('-f','--filename', metavar='<filename>', help='CSV file with datas')
    arg_parser.add_argument('filename', nargs='+')
    args = arg_parser.parse_args()
    datas = []
    title = 'Convergence for Client 1: {}ms Client 2: {}ms Against Sync Messages'
    this_delay2 = None
    this_type = None
    pyplot.xlabel('Time (ms)')
    pyplot.ylabel('Max Client Delay (ms)')
    for f,c in zip(args.filename, color_vals):
        print('PARSING {}...'.format(f))
        type = os.path.split(f)[0]
        delay1, delay2, syncs = parse_filename(f)
        if this_delay2 is None:
            this_delay2 = delay2
            this_type = type
            pyplot.title(title.format(delay1, delay2))
        else:
            assert delay2 == this_delay2
            if type not in this_type:
                this_type.append(type)
        with open(f, 'r') as file:
            data = parse_csv(file)
        data = format_data(data)
        data.delay1 = delay1
        data.delay2 = delay2
        data.syncs = syncs
        data.figname = os.path.splitext(f)[0] + suffix
        datas.append(data)
        #axes = data.plot(x='time', y='max client delay', kind='scatter', c=c, title=title)
        pyplot.plot(data['time'], data['max client delay'], '-o', c=c, label='{}-{}'.format(type,syncs))
    legend = pyplot.legend(loc=9, bbox_to_anchor=(0.5, -0.1), title='Sync Messages', ncol=len(args.filename))
    axes = pyplot.gca()
    axes.set_xlim(left = -3000)
    pyplot.xlabel('Time (ms)')
    pyplot.ylabel('Max Client Delay (ms)')
    pyplot.savefig('{}_{}{}'.format(this_type,this_delay2, suffix), bbox_inches='tight');
    pyplot.close()
    for d in datas:
        plot_data(data)
    return 0

if __name__ == '__main__':
    rtn = main()
    sys.exit(rtn)
