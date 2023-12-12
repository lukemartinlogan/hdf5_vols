//
// Created by lukemartinlogan on 12/8/23.
//

#include <iostream>
#include <H5Ppublic.h>
#include <hdf5.h>
#include <vector>
#include "H5VLreplicate_vol.h"

int main() {
  if (H5open() < 0) {
    printf("H5open failed\n");
    return 1;
  }

  hsize_t dims[2] = {32, 32};
  std::vector<int> write_data(dims[0] * dims[1], 0);
  std::vector<int> read_data(dims[0] * dims[1], 1);
  hid_t file_id = H5Fcreate("mydata.txt", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  hid_t dataspace_id = H5Screate_simple(2, dims, NULL);
  hid_t dataset_id = H5Dcreate2(file_id, "mydataset", H5T_NATIVE_INT,
                                dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, write_data.data());
  H5Dread(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, read_data.data());
  H5Sclose(dataspace_id);
  H5Dclose(dataset_id);
  H5Fclose(file_id);

//  void *info;
//  hid_t vol = H5VLregister_connector_by_name("replicate_vol", H5P_DEFAULT);
//  H5VLconnector_str_to_info("asdf asdf asdf asdf", vol, &info);

//  hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
//  hid_t vol = H5VLregister_connector_by_name("replicate_vol", H5P_DEFAULT);
//  H5Pset_vol(fapl, vol, "replicate_vol asfaskfjasklf asdfasflkas;f");
}