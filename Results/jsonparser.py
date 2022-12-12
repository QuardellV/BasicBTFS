import json
import os
import csv

import pathlib

import pandas as pandas
import plotly.express as px
from kaleido.scopes.plotly import PlotlyScope
scope = PlotlyScope()
import subprocess
import yaml
import string
import copy

size_lookup = {
    "K" : 1000,
    "M" : 1000000
}

def get_size(data):
    cur_size = data["global options"]["size"]
    multiplier = size_lookup.get(cur_size[-1])
    if (isinstance(multiplier, int)) :
        size_int = int(cur_size[:-1])
        if (isinstance(size_int, int)) :
            return size_int * multiplier
    return None

def get_size_verbose(data):
    return data["global options"]["size"]

def get_read_latency_mean(data):
    return data["jobs"][0]["read"]["lat_ns"]["mean"]

def get_read_bw_mean(data):
    return data["jobs"][0]["read"]["bw_mean"]

def get_read_bw(data):
    return data["jobs"][0]["read"]["bw"]

def get_read_iops(data):
    return data["jobs"][0]["read"]["iops"]

def get_read_runtime(data):
    return data["jobs"][0]["read"]["runtime"]

def get_write_bw(data):
    return data["jobs"][0]["write"]["bw"]

def get_write_iops(data):
    return data["jobs"][0]["write"]["iops"]

def get_write_runtime(data):
    return data["jobs"][0]["write"]["runtime"]

def get_write_latency_mean(data):
    return data["jobs"][0]["write"]["lat_ns"]["mean"]

def get_write_bw_mean(data):
    return data["jobs"][0]["write"]["bw_mean"]

def handle_file(file, writer, config):
    try:
        with open(file) as f:
            cur_data = json.load(f)
            cur_row = [get_size(cur_data), 
                       get_size_verbose(cur_data),
                       get_read_bw_mean(cur_data),
                       get_read_latency_mean(cur_data),
                       get_read_bw(cur_data),
                       get_read_iops(cur_data),
                       get_write_bw_mean(cur_data),
                       get_write_latency_mean(cur_data),
                       get_write_bw(cur_data),
                       get_write_iops(cur_data),
                       config
                       ]
            writer.writerow(cur_row)
            f.close()
        print("Succesfully processed file %s" % file)

    except json.decoder.JSONDecodeError:
        print("something went wrong opening file %s. Corrupt file due to too large for filesystem" % file)

def handle_fs_dir(dir, writer, config):
    for size_dirname in sorted(os.listdir(dir)):
        size_dir = os.path.join(dir, size_dirname)

        for iteration in os.listdir(size_dir):
            iteration_dir = os.path.join(dir, iteration)

            for filename in os.listdir(iteration_dir):
                file = os.path.join(size_dir, filename)

                if os.path.isfile(file) and file.endswith(".output"):
                    handle_file(file, writer, config)

def handle_benchmark(type, header):
    bench_dir = os.path.join('tmpfs', type)
    for dirname in os.listdir(bench_dir):
        fs_dir = os.path.join(bench_dir,dirname)
        if os.path.isdir(fs_dir):
            print(fs_dir)
            csv_name = type + "_" + dirname + '.csv'
            with open(csv_name, 'w', encoding='UTF8') as csvfile:
                writer = csv.writer(csvfile, delimiter=',', quotechar='|', quoting=csv.QUOTE_MINIMAL)
                writer.writerow(header)
                handle_fs_dir(fs_dir, writer, dirname)
                csvfile.close()

def aggregrate_df(csv_file):
    df = pandas.read_csv(csv_file, sep="\t")
    df_final = copy.copy(df)
    df_final = df_final[0:0]

    for group_name, df_group in df.groupby(["size", "size_verbose", "config"]):
        new_row = [group_name[0], group_name[1], df_group["r_bw_mean"].mean(), df_group['r_lat_mean'].mean(),
                    df_group['r_bw'].mean(), df_group['r_iops'].mean(),df_group['w_bw_mean'].mean(), df_group['w_lat_mean'].mean(),
                    df_group['w_bw'].mean(), df_group['w_iops'].mean(), group_name[2]]
        df_final.loc[len(df_final)] = new_row
    return df_final

def plot_fig(df, x, y, name):
    template = "plotly_white"
    output_dir = ""

    fig = px.line(df, x=x, y=y, color="config", template=template,
              labels={
                  x: "Size [Bytes]", y: "CPU usage [%]", "config": "Filesystem"
              })

    with open(str(output_dir.joinpath("%s.pdf" % name)), "wb") as f:
        f.write(scope.transform(fig, format="pdf"))

def plot_bw():
    frames_bw = []
    frames_lat = []

    for csv in os.listdir("csv"):
        csv_file = os.path.join("csv", csv)

        if os.path.isfile(csv_file) and csv_file.endswith("bw.csv"):
            frames_bw.append(aggregrate_df(csv_file))
        elif os.path.isfile(csv_file) and csv_file.endswith("lat.csv"):
            frames_lat.append(aggregrate_df(csv_file))

    df_bw = pandas.concat(frames_bw)
    df_lat = pandas.concat(frames_lat)

    plot_fig(df_bw, "size", "r_bw", "Read_Bandwidth")
    plot_fig(df_bw, "size", "w_bw", "Write_Bandwidth")
    plot_fig(df_lat, "size", "r_lat", "Read_Latency")
    plot_fig(df_lat, "size", "w_lat", "Write_Latency")

if __name__=="__main__":

    bw_dir  = 'bandwidth'
    lat_dir = 'latency'
    header = ['size', 'size_verbose', 'r_bw_mean', 'r_lat_mean', 'r_bw', 'r_iops','w_bw_mean', 'w_lat_mean', 'w_bw', 'w_iops', "config"]

    handle_benchmark(bw_dir, header)
    handle_benchmark(lat_dir, header)


