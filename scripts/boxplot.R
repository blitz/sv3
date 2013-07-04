#!/usr/bin/Rscript

args <- commandArgs(trailingOnly = TRUE)

input <- read.csv(args[1], header = FALSE)
rownames(input) <- input[,1]
input[,1] <- NULL

pdf(paste(args[1], ".pdf", sep=""))
boxplot.matrix(as.matrix(input), use.cols=FALSE, ylab="Roundtrip in us")
dev.off()
