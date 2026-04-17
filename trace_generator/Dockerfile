
FROM eclipse-temurin:8-jdk


RUN apt-get update && apt-get install -y ant cpp git

WORKDIR /app

RUN git clone https://github.com/stephenfreund/RoadRunner.git

RUN cd RoadRunner && ant

# Set paths and variables
ENV JVM_ARGS="-Xmx4g -Xms1g"
ENV RR_MODE=FAST
ENV RR_HOME=/app/RoadRunner
ENV RR_TOOLPATH=$RR_HOME/classes/rr/simple:$RR_HOME/classes/tools
ENV PATH=$PATH:$RR_HOME/bin:$RR_HOME/auto/bin

COPY init-rr.sh /init-rr.sh
RUN chmod +x /init-rr.sh

# Configure bash to load the dynamic variables for new sessions
RUN echo 'if [ -f /etc/dynamic_env.sh ]; then . /etc/dynamic_env.sh; fi' >> /etc/bash.bashrc && \
    echo 'if [ -f /etc/dynamic_env.sh ]; then . /etc/dynamic_env.sh; fi' >> /root/.bashrc

ENTRYPOINT ["/init-rr.sh"]
CMD ["/bin/bash"]