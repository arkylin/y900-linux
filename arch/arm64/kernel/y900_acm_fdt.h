static int __init y900_acm_first_memory(const void *blob)
{
 int offset;
 const char *type;

 fdt_for_each_subnode(offset, blob, 0) {
  type = fdt_getprop(blob, offset, "device_type", NULL);
  if (type && !strcmp(type, "memory"))
   return offset;
 }
 return -FDT_ERR_NOTFOUND;
}

static int __init y900_acm_copy_property(const void *source, int source_node,
                                       void *target, int target_node,
                                       const char *name)
{
 const void *value;
 int length;

 value = fdt_getprop(source, source_node, name, &length);
 if (!value)
  return length == -FDT_ERR_NOTFOUND ? 0 : length;
 return fdt_setprop(target, target_node, name, value, length);
}

static int __init y900_acm_merge_fdt(const void *firmware, const void *board,
                                   void *target, int capacity)
{
 static const char * const chosen_properties[] = {
  "bootargs", "kaslr-seed", "rng-seed", "linux,usable-memory-range",
 };
 int result, node, target_node, parent, length, index, memory_count = 0;
 int firmware_chosen, target_chosen, firmware_reserved;
 const void *region, *existing;
 const char *type, *status;
 int existing_length;
 uint64_t address, size;
 char name[64];

 result = fdt_check_header(firmware);
 if (result)
  return result;
 result = fdt_check_header(board);
 if (result)
  return result;
 if (fdt_address_cells(firmware, 0) != 2 ||
     fdt_size_cells(firmware, 0) != 2 ||
     fdt_address_cells(board, 0) != 2 ||
     fdt_size_cells(board, 0) != 2)
  return -FDT_ERR_BADNCELLS;
 result = fdt_open_into(board, target, capacity);
 if (result)
  return result;
 while ((node = y900_acm_first_memory(target)) >= 0) {
  result = fdt_del_node(target, node);
  if (result)
   return result;
 }
 fdt_for_each_subnode(node, firmware, 0) {
  type = fdt_getprop(firmware, node, "device_type", NULL);
  if (!type || strcmp(type, "memory"))
   continue;
  region = fdt_getprop(firmware, node, "reg", &length);
  if (!region || length <= 0 || length % 16)
   return -FDT_ERR_BADVALUE;
  target_node = fdt_add_subnode(target, 0, fdt_get_name(firmware, node, NULL));
  if (target_node < 0)
   return target_node;
  result = fdt_setprop(target, target_node, "reg", region, length);
  if (result)
   return result;
  result = fdt_setprop_string(target, target_node, "device_type", "memory");
  if (result)
   return result;
  result = y900_acm_copy_property(firmware, node, target, target_node, "status");
  if (result)
   return result;
  memory_count++;
 }
 if (!memory_count)
  return -FDT_ERR_NOTFOUND;
 firmware_chosen = fdt_path_offset(firmware, "/chosen");
 if (firmware_chosen >= 0) {
  for (index = 0; index < (int)(sizeof(chosen_properties) /
                              sizeof(chosen_properties[0])); index++) {
   target_chosen = fdt_path_offset(target, "/chosen");
   if (target_chosen < 0)
    return target_chosen;
   result = y900_acm_copy_property(firmware, firmware_chosen, target,
                                   target_chosen, chosen_properties[index]);
   if (result)
    return result;
  }
 }
 for (index = 0; index < fdt_num_mem_rsv(firmware); index++) {
  result = fdt_get_mem_rsv(firmware, index, &address, &size);
  if (result)
   return result;
  result = fdt_add_mem_rsv(target, address, size);
  if (result)
   return result;
 }
 firmware_reserved = fdt_path_offset(firmware, "/reserved-memory");
 if (firmware_reserved >= 0) {
  if (fdt_address_cells(firmware, firmware_reserved) != 2 ||
      fdt_size_cells(firmware, firmware_reserved) != 2)
   return -FDT_ERR_BADNCELLS;
  fdt_for_each_subnode(node, firmware, firmware_reserved) {
   status = fdt_getprop(firmware, node, "status", NULL);
   if (status && strcmp(status, "okay") && strcmp(status, "ok"))
    continue;
   region = fdt_getprop(firmware, node, "reg", &length);
   if (!region) {
    static const char * const allocation_properties[] = {
     "size", "alignment", "alloc-ranges", "no-map",
    };

    if (!fdt_getprop(firmware, node, "size", NULL))
     continue;
    parent = fdt_path_offset(target, "/reserved-memory");
    if (parent < 0)
     return parent;
    snprintf(name, sizeof(name), "firmware-allocation-%x", node);
    target_node = fdt_add_subnode(target, parent, name);
    if (target_node < 0)
     return target_node;
    for (index = 0; index < (int)(sizeof(allocation_properties) /
                                sizeof(allocation_properties[0])); index++) {
     result = y900_acm_copy_property(firmware, node, target,
                                     target_node, allocation_properties[index]);
     if (result)
      return result;
    }
    continue;
   }
   if (length <= 0 || length % 16)
    return -FDT_ERR_BADVALUE;
   parent = fdt_path_offset(target, "/reserved-memory");
   if (parent < 0)
    return parent;
   target_node = -FDT_ERR_NOTFOUND;
   fdt_for_each_subnode(index, target, parent) {
    existing = fdt_getprop(target, index, "reg", &existing_length);
    if (existing && existing_length == length && !memcmp(existing, region, length)) {
     target_node = index;
     break;
    }
   }
   if (target_node < 0) {
    snprintf(name, sizeof(name), "firmware-reserved-%x", node);
    target_node = fdt_add_subnode(target, parent, name);
    if (target_node < 0)
     return target_node;
    result = fdt_setprop(target, target_node, "reg", region, length);
    if (result)
     return result;
   }
   if (fdt_getprop(firmware, node, "no-map", NULL)) {
    fdt_delprop(target, target_node, "reusable");
    result = fdt_setprop(target, target_node, "no-map", NULL, 0);
    if (result)
     return result;
   }
  }
 }
 return fdt_pack(target);
}

