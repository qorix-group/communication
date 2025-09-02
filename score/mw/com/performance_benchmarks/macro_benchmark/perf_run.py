import os
import subprocess
import sys
import json
import shutil


def clean_up(mw_com_config_path: str):
    with open(mw_com_config_path, "r") as mw_com_config_fp:
        mw_com_config = json.load(mw_com_config_fp)
        # NOTE: this is not a general way to parse the mw_com_config and will
        # stop working if the config is changed
        service_id = mw_com_config["serviceTypes"][0]["bindings"][0]["serviceId"]
        instance_id = mw_com_config["serviceInstances"][0]["instances"][0]["instanceId"]
        tag = f"{service_id:016}-{instance_id:05}"
        cleanup_list = [f"lola-ctl-{tag}", f"lola-ctl-{tag}-b", f"lola-data-{tag}"]

        for shm_file in cleanup_list:
            shm_path = f"/dev/shm/{shm_file}"
            if os.path.isfile(shm_path):
                os.remove(shm_path)

    service_discovery_path = f"/tmp/mw_com_lola/service_discovery/{service_id}"
    if os.path.isdir(service_discovery_path):
        shutil.rmtree(service_discovery_path)


def main():
    # Parse command-line arguments
    if len(sys.argv) != 3:
        print("Usage: python script.py <LOGGING_JSON_PATH>",
              "<path/to/config_dir> ")
        print("config_dir will usually be an output artifact of a bazel.")
        sys.exit(1)

    run_dir = "/".join(sys.argv[0].split("/")[0:-1])
    config_dir = sys.argv[2]
    client_mw_com_config_path = f"{run_dir}/{config_dir}/client_mw_com_config.json"
    service_mw_com_config_path = f"{run_dir}/{config_dir}/service_mw_com_config.json"

    client_config_path = f"{run_dir}/{config_dir}/client_benchmark_config.json"
    service_config_path = f"{run_dir}/{config_dir}/service_benchmark_config.json"

    service_path = f"{run_dir}/lola_benchmarking_service"
    client_path = f"{run_dir}/lola_benchmarking_client"

    service_proc = subprocess.Popen([service_path,
                                     service_config_path,
                                     client_mw_com_config_path])
    service_pid = service_proc.pid

    subprocess.Popen(["perf", "record", "-F", "99", "-a", "-g", "-p",
                      str(service_pid), "-o", "data_service.perf"])

    client_proc = subprocess.Popen([client_path,
                                    client_config_path,
                                    service_mw_com_config_path])
    client_pid = client_proc.pid

    subprocess.Popen(["perf", "record", "-F", "99", "-a", "-g", "-p",
                      str(client_pid),
                      "-o", "data_client.perf"])

    service_proc.wait()
    client_proc.wait()

    clean_up(client_mw_com_config_path)
    print("All Done")


if __name__ == "__main__":
    main()
