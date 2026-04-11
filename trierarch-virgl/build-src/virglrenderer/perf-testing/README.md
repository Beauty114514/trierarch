The files in this directory help with testing Virgl on the virtio-gpu winsys 
by means of Crosvm.

A whole environment will be built in a Docker image, then Mesa and Virglrenderer 
will be built from local directories to be used both in the host and the guest.

The container image builds on top of other images built by scripts in the crosvm repository.

Instructions for building base images:

```console
$ git clone https://chromium.googlesource.com/chromiumos/platform/crosvm
$ pushd crosvm
$ sh docker/build_crosvm_base.sh
$ sh docker/build_crosvm.sh
```

Instructions for building target image:

```console
$ cd virglrenderer
$ sh perf-testing/build-dockerimage.sh
```

Instructions for running the container:

```console
$ cd virglrenderer
$ bash perf-testing/run_trace-in-container.sh \
    --root $PATH_THAT_CONTAINS_MESA_CHECKOUT_VIRGLRENDERER_AND_TRACES_DB_CHECKOUT \
    --trace $API_TRACE_TO_RUN
```

There are also options for run_trace-in-container.sh that allow specifying the 
path to mesa, virglrenderer, and the traces db. These override the root path. 
In addition, the root path defaults to the current working directory. 

As a conveniance for shell autocompletion users running the script from the default 
root that contains the traces db as subdirectory the the trace file name can be 
also given with this traces db sudirectory name, i.e. if the traces db is located 
in '$workdir/traces-db', root=$workdir,  and the trace is calles 'sometrace.trace', 
then both commands 
```
  perf-testing/run_trace-in-container.sh -r $rootdir -t traces-db/sometrace.trace
```
and 
```
  perf-testing/run_trace-in-container.sh -r $rootdir -t sometrace.trace
```
will work equally. 

At the moment of writing, the branch perfetto-tracing is needed for mesa at
commit ec4277aea63cf3, and the for virglrenderer at least commit
"perf: compile mesa with perfeto support" is needed so that these projects
emit the required traces.

The perfetto traces will be saved to the a subdirectory of the traces-db checkout 
directory with a name based on the api trace passed in with the --trace parameter. 

Once the run_trace-in-container.sh script finishes, 3 Perfetto trace files will be written: 
$(API_TRACE_TO_RUN%.*}-host.perfetto, $(API_TRACE_TO_RUN%.*}-guest.perfetto 
and $(API_TRACE_TO_RUN%.*}-summary.perfetto. The last one is the fusion of the two first.

In order to visualize the traces, the Perfetto UI needs to be running in a local 
service which can be started as follows:

```console
$ perf-testing/perfetto-ui.sh
```

The Perfetto UI can be loaded then on Chromium on the http://localhost:10000 address.
