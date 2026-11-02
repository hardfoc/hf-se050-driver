/**
 * @file ports.cpp
 * @brief Instantiates every driver class over the documented port, so the whole API compiles
 * against it.
 */
#include "my_i2c.hpp"
#include "se050_driver.hpp"

template class se050::Device<MyI2c>;
template class se050::Session<MyI2c>;
template class se050::T1Session<MyI2c>;
template class se050::scp03::Session<MyI2c>;
