#' RcppBoostCompute
#'
#' @name RcppBoostCompute
#' @docType package
#' @useDynLib RcppBoostCompute
#' @import Rcpp
NULL

#' RcppBoostCompute linker flags
#' 
#' @description Returns required system linker flags to link OpenCL library.
#' @return A string
#' @export
LdFlags <- function() {
    system <- Sys.info()["sysname"]
    if (system == "Darwin") {
        flags <- "-framework OpenCL"
    } else if (system == "Linux") {
        flags <- "-lOpenCL"
    } else {
        stop("Unsupport system for RcppBoostCompute")
    }
    if (length(CppFlags()) == 0) {
        flags <- ""
    }
    cat(flags)
}

#' RcppBoostCompute preprocessor flags
#' 
#' @description Returns preprocessor flags to specific OpenCL availability.
#' @return A string
#' @export
CppFlags <- function() {
    cat(.cppFlags())
}

#' Print available OpenCL devices
#'
#' @name compute_hello_world
#'
#' @description
#' Lists the available OpenCL devices on system.
#'
#' @export compute_hello_world
NULL