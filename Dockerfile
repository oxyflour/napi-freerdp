FROM node:14

RUN sed -i "s@http://.*.debian.org@http://mirrors.aliyun.com@g" /etc/apt/sources.list
RUN apt update && apt install -y cmake libfreerdp-dev

# for cmake-js
ENV NVM_NODEJS_ORG_MIRROR https://npm.taobao.org/dist
RUN npm set registry https://r.npm.taobao.org
