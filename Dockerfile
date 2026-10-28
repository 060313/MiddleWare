# SPDX-License-Identifier: MIT
# 开发 / x86_64 仿真镜像（openEuler 24.03-LTS，与目标平台一致）
#
# 用法：
#   docker build -t schedd-dev:latest .
#   docker run --rm -it -v "$PWD":/work -w /work schedd-dev:latest bash
#
# 注：本框架使用手写 Makefile 构建，镜像内不安装 CMake。

FROM openeuler/openeuler:24.03-lts

LABEL description="CPU+XPU heterogeneous scheduling middleware dev image (make-based)"

RUN dnf install -y \
        gcc gcc-c++ make git tar gzip which \
    && dnf clean all \
    && rm -rf /var/cache/dnf

ENV ONNXRUNTIME_ROOT=/reference/onnxruntime-linux-x64-1.20.1

WORKDIR /work

CMD ["/bin/bash"]
