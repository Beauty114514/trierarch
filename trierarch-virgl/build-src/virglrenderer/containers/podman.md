# Accelerated graphics inside a container

Even though the VirGL OpenGL driver is usually used inside a QEMU virtual machine, it can used in conjonction with the vtest server to provide a virtual GPU inside a container.

vtest currently uses a UNIX socket to pass the rendering commands to the server.

## Container

To create a base Ubuntu/RHEL9 container that contains the latest `virgl` OpenGL driver, you can use the Dockerfile provided and overlay your applications/dependencies are needed.

```shell
podman build -f Dockerfile.${DISTRO}
```

However, please note that there is no requirement to use these as long as the container contains a recent-enough version of the VirGL and swrast drivers ([Mesa](https://gitlab.freedesktop.org/mesa/mesa)).

## Running

Run in a separate terminal: `virgl_test_server`

It is also possible to use the [Podman OCI hooks](https://gitlab.freedesktop.org/virgl/podman-virgl-hook) to automatically start and stop the server component on demand. It should work as long as the container is started with the annotation `virgl=enabled`.

Now start the container using the provided script:

```shell
./run-container.sh ${CONTAINER_ID}
```

You should the be able to start `glxgears` using the VirGL driver to render (`GL_RENDERER = virgl` should be displayed at the top of the output):

```shell
glxgears -info
```
