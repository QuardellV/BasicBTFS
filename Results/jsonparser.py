import json
import os
import csv

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

def handle_file(file, writer):
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
                       get_write_iops(cur_data)
                       ]
            writer.writerow(cur_row)
            f.close()
        print("Succesfully processed file %s" % file)
    
    except json.decoder.JSONDecodeError:
        print("something went wrong opening file %s. Corrupt file due to too large for filesystem" % file)

def handle_fs_dir(dir, writer):
    for size_dirname in sorted(os.listdir(dir)):

        size_dir = os.path.join(dir, size_dirname)

        for filename in os.listdir(size_dir):
            file = os.path.join(size_dir, filename)

            if os.path.isfile(file) and file.endswith(".output"):
                handle_file(file, writer)

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
                handle_fs_dir(fs_dir, writer)
                csvfile.close()




if __name__=="__main__":

    bw_dir  = 'bandwidth'
    lat_dir = 'latency'
    header = ['size', 'size_verbose', 'r_bw_mean', 'r_lat_mean', 'r_bw', 'r_iops','w_bw_mean', 'w_lat_mean', 'w_bw', 'w_iops']

    handle_benchmark(bw_dir, header)
    handle_benchmark(lat_dir, header)
