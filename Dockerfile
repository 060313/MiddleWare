# 开发 / x86_64 仿真镜像（openEuler 24.03-LTS，与目标平台一致）
# 用法：
#   docker build -t schedd-dev:latest .
#   docker run --rm -it -v "$PWD":/work -w /work schedd-dev:latest bash
FROM openeuler/openeuler:24.03-lts

LABEL description="CPU+XPU heterogeneous scheduling middleware dev image"

RUN dnf install -y \
        gcc gcc-c++ make cmake git tar gzip which \
    && dnf clean all \
    && rm -rf /var/cache/dnf

# ONNX Runtime 为可选依赖：把解压后的 SDK 放到 /opt/onnxruntime 即可被 CMake 自动找到
ENV ONNXRUNTIME_ROOT=/opt/onnxruntime

WORKDIR /work

CMD ["/bin/bash"]
