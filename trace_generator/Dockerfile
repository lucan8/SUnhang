
FROM eclipse-temurin:8-jdk


RUN apt-get update && apt-get install -y ant cpp git


WORKDIR /app

RUN git clone https://github.com/stephenfreund/RoadRunner.git

RUN cd RoadRunner && ant


ENV RR_HOME=/app/RoadRunner
ENV PATH=$PATH:$RR_HOME/bin


CMD ["/bin/bash"]