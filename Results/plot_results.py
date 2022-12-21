import json
import os
import csv

import pathlib

import pandas as pandas
import plotly.express as px
from kaleido.scopes.plotly import PlotlyScope
import matplotlib.pyplot as plt
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
                       str(config),
                       ]
            writer.writerow(cur_row)
            f.close()
        print("Succesfully processed file %s" % file)

    except json.decoder.JSONDecodeError:
        print("something went wrong opening file %s. Corrupt file due to too large for filesystem" % file)

def handle_fs_dir(dir, writer, config):
    for size_dirname in sorted(os.listdir(dir)):
        size_dir = os.path.join(dir, size_dirname)

        for i in range(21):
            iteration_dir = os.path.join(size_dir, str(i))

            for filename in os.listdir(iteration_dir):
                file = os.path.join(iteration_dir, filename)
                print(file)
                
                if os.path.isfile(file) and file.endswith(".output"):
                    handle_file(file, writer, config)

def handle_benchmark(type, header):
    bench_dir = os.path.join('tmpfs', type)
    for dirname in os.listdir(bench_dir):
        fs_dir = os.path.join(bench_dir,dirname)
        if os.path.isdir(fs_dir):
            csv_name = type + "_" + dirname + '.csv'
            csv_name = os.path.join("csv", csv_name)
            with open(csv_name, 'w', encoding='UTF8', newline='') as csvfile:
                writer = csv.writer(csvfile, delimiter=',', quotechar='|')
                writer.writerow(header)
                handle_fs_dir(fs_dir, writer, dirname)
                csvfile.close()

def print_debug(df):
    print("bandwidth read %s: %f" % ("min", df["r_bw_mean"].min()))
    print("bandwidth read %s: %f" % ("max", df["r_bw_mean"].max()))
    print("bandwidth read %s: %f" % ("mean", df["r_bw_mean"].mean()))

    print("bandwdith write %s: %f" % ("min", df["w_bw_mean"].min()))
    print("bandwdith write %s: %f" % ("max", df["w_bw_mean"].max()))
    print("bandwidth write %s: %f" % ("mean", df["w_bw_mean"].mean()))

    print("latency read %s: %f" % ("min", df["r_lat_mean"].min()))
    print("latency read %s: %f" % ("max", df["r_lat_mean"].max()))
    print("latency read %s: %f" % ("mean", df["r_lat_mean"].mean()))
    
    print("latency write %s: %f" % ("min", df["w_lat_mean"].min()))
    print("latency write %s: %f" % ("max", df["w_lat_mean"].max()))
    print("latency write %s: %f" % ("mean", df["w_lat_mean"].mean()))


def aggregrate_df(df):
    df_final = copy.copy(df)
    df_final = df_final[0:0]


    for group_name, df_group in df.groupby(['size', 'size_verbose', 'config']):
        print("------------------------------- current config: (%s, %s) -------------------------------" % (group_name[1], group_name[2]))
        print_debug(df_group)
        new_row = [group_name[0], group_name[1], df_group["r_bw_mean"].mean(), df_group['r_lat_mean'].mean(),
                    df_group['r_bw'].mean(), df_group['r_iops'].mean(),df_group['w_bw_mean'].mean(), df_group['w_lat_mean'].mean(),
                    df_group['w_bw'].mean(), df_group['w_iops'].mean(), group_name[2]]
        df_final.loc[len(df_final)] = new_row
    return df_final

def plot_fig_avg(df, x, y, name, config_list : dict, ylabel):
    template = "plotly_white"
    output_dir = ""
    xtick_set = False
    fig = px.line(df, x=x, y=y, color="config", template=template,
              labels={
                  x: "Size [Bytes]", y: "CPU usage [%]", "config": "Filesystem"
              })

    tmp = ("%s.pdf" % name)
    filename = os.path.join(output_dir, tmp)
    with open(filename, "wb") as f:
        f.write(scope.transform(fig, format="pdf"))

    plt.rcParams.update({'font.size': 12})
    plt.rcParams["figure.figsize"] = (10,5)


    for config, label in config_list.items():
        cur_df = df[(df["config"] == config)]
        # print("------------------------------- current config: %s -------------------------------" % config)
        # print(cur_df)
        plt.plot(cur_df[x], cur_df[y], label=label)

        if not xtick_set:
            plt.xscale('log', base=2)
            plt.xticks(cur_df[x], cur_df["size_verbose"].transform(lambda x: x + "B"))
            xtick_set = True

    
    plt.legend()
    plt.xlabel("Size [Bytes]")
    
    plt.ylabel(ylabel)

    tmp = ("%s_plt_median.pdf" % name)
    filename = os.path.join(output_dir, tmp)
    plt.savefig(filename, bbox_inches='tight')
    plt.show()

def plot():
    frames_bw = []
    frames_lat = []

    configs_bw = {"btfsbw" : "BasicBtreeFS", 
                  "btfsnocachebw" : "BasicBtreeFSNoCache", 
                  "ftfsbw" : "BasicLinkFS", 
                  "linbtrfsbw" : "Linux BTRFS", 
                  "linfatfsbw" : "Linux FAT/MSDOS"}
    configs_lat = {"btfslat" : "BasicBtreeFS", 
                   "btfsnocachelat" : "BasicBtreeFSNoCache", 
                   "ftfslat" : "BasicLinkFS", 
                   "linbtrfslat" : "Linux BTRFS", 
                   "linfatfslat" : "Linux FAT/MSDOS"}

    for csv in os.listdir("csv"):
        csv_file = os.path.join("csv", csv)

        if os.path.isfile(csv_file) and csv_file.endswith("bw.csv"):
            df = pandas.read_csv(csv_file, delimiter=',', quotechar='|')
            frames_bw.append(aggregrate_df(df))
        elif os.path.isfile(csv_file) and csv_file.endswith("lat.csv"):
            df = pandas.read_csv(csv_file, delimiter=',', quotechar='|')
            frames_lat.append(aggregrate_df(df))
        
    df_bw = pandas.concat(frames_bw)
    df_lat = pandas.concat(frames_lat)
    
    df_bw["r_bw_mean"] = df_bw["r_bw_mean"].apply(pandas.to_numeric).transform(lambda x: x / (1000 * 1000))
    df_bw["w_bw_mean"] = df_bw["w_bw_mean"].apply(pandas.to_numeric).transform(lambda x: x / (1000 * 1000))
    df_lat["r_lat_mean"] = df_lat["r_lat_mean"].apply(pandas.to_numeric).transform(lambda x: x / 1000)
    df_lat["w_lat_mean"] = df_lat["w_lat_mean"].apply(pandas.to_numeric).transform(lambda x: x / 1000)

    plot_fig_avg(df_bw, "size", "r_bw_mean", "Read_Bandwidth", configs_bw, "Bandwidth [GiB/s]")
    plot_fig_avg(df_bw, "size", "w_bw_mean", "Write_Bandwidth", configs_bw, "Bandwidth [GiB/s]")
    plot_fig_avg(df_lat, "size", "r_lat_mean", "Read_Latency", configs_lat, "Latency [\u03bcsec]")
    plot_fig_avg(df_lat, "size", "w_lat_mean", "Write_Latency", configs_lat, "Latency [\u03bcsec]")


if __name__=="__main__":

    bw_dir  = 'bandwidth'
    lat_dir = 'latency'
    header = ['size', 'size_verbose', 'r_bw_mean', 'r_lat_mean', 'r_bw', 'r_iops','w_bw_mean', 'w_lat_mean', 'w_bw', 'w_iops', "config"]

    handle_benchmark(bw_dir, header)
    handle_benchmark(lat_dir, header)
    plot()


