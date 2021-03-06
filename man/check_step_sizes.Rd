% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/minc_interface.R
\name{check_step_sizes}
\alias{check_step_sizes}
\title{Check file step sizes}
\usage{
check_step_sizes(filenames, atlas = NULL, strict = FALSE,
  tolerance = 1e-05)
}
\arguments{
\item{filenames}{A character vector of minc file names}

\item{atlas}{An optional atlas to compare against the files}

\item{strict}{Should differences be treated as errors or warnings}

\item{tolerance}{The tolerance for comparing step sizes, 10e-6 by default}
}
\value{
A 3-component vector of step sizes.
}
\description{
Check if a collection of minc files all have the same step size.
}
