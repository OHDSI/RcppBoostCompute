library(testthat)

test_that("Hello world", {
    string <- compute_hello_world()
    expect_null(string)
})