#include "compression_reader.h"
#include <iostream>
#include <fstream>
#include <sstream>
// ***************************************************************************************************************************************
// ***************************************************************************************************************************************
bool CompressionReader::OpenForReading(string &file_name)
{
    if (merge_flag)
    {
        bcf_hdr_t *temp_vcf_hdr = nullptr;
        vector<string> merge_file_names;
        long size = 0;
        if (file_name[0] == '@')
        {
            std::ifstream directory_file_name(file_name.substr(1));
            if (!directory_file_name.is_open())
            {
                std::cerr << "Error. Cannot open " << file_name.substr(1) << " file with samples.\n";
                exit(1);
            }
            std::string item;

            while (directory_file_name >> item)
            {
                merge_file_names.emplace_back(item);
                size++;
            }
            std::cerr << "size:" << size << endl;
            directory_file_name.clear();
        }
        else
        {
            char delim = ',';
            std::stringstream ss(file_name);
            std::string item;
            while (getline(ss, item, delim))
            {
                merge_file_names.emplace_back(item);
                size++;
            }
        }
        merge_files.resize(size);
        for (long i = 0; i < size; i++)
        {

            if (merge_files[i])
                hts_close(merge_files[i]);
            if (in_type == file_type::VCF_File)
            {
                merge_files[i] = hts_open(merge_file_names[i].c_str(), "r");
            }
            else
            {
                merge_files[i] = hts_open(merge_file_names[i].c_str(), "rb");
            }
            if (!merge_files[i])
            {
                std::cerr << "could not open " << merge_file_names[i] << " file" << std::endl;
                merge_failure_flag = true;
                return false;
            }
            hts_set_opt(merge_files[i], HTS_OPT_CACHE_SIZE, 32 << 20);
            hts_set_opt(merge_files[i], HTS_OPT_BLOCK_SIZE, 32 << 20);

            temp_vcf_hdr = bcf_hdr_read(merge_files[i]);
            vcf_hdr = bcf_hdr_merge(vcf_hdr, temp_vcf_hdr);
        }
        vcf_record = bcf_init();
    }
    else
    {
        if (in_file)
            hts_close(in_file);

        if (in_type == file_type::VCF_File)
        {
            in_file = hts_open(file_name.c_str(), "r");
        }
        else
        {
            in_file = hts_open(file_name.c_str(), "rb");
        }
        if (!in_file)
        {
            std::cerr << "could not open " << in_file_name << " file" << std::endl;
            return false;
        }
        hts_set_opt(in_file, HTS_OPT_CACHE_SIZE, 32 << 20);   // 数据批量读入缓存中，减少频繁的磁盘 I/O 操作
        hts_set_opt(in_file, HTS_OPT_BLOCK_SIZE, 32 << 20);
        if (vcf_hdr)
            bcf_hdr_destroy(vcf_hdr);
        vcf_hdr = bcf_hdr_read(in_file);
        int thread_count = 4;
        hts_set_threads(in_file, thread_count);
        vcf_record = bcf_init();    // 存储从文件中读取的每一条记录
    }
    return true;
}

// ***************************************************************************************************************************************
// Get number of samples in VCF
bool CompressionReader::ReadFile()
{

    if (!(in_file || !merge_failure_flag) || !vcf_hdr)
        return -1;

    no_samples = bcf_hdr_nsamples(vcf_hdr);
    if (!vcf_hdr_read)
    {

        for (size_t i = 0; i < no_samples; i++)
            samples_list.emplace_back(vcf_hdr->samples[i]);

        vcf_hdr_read = true;
    }
    return true;
}
// Get number of samples in VCF
// ***************************************************************************************************************************************
uint32_t CompressionReader::GetSamples(vector<string> &s_list)
{

    if (!vcf_hdr_read)
    {

        ReadFile();
    }
    s_list = samples_list;

    return no_samples;
}

// ***************************************************************************************************************************************
bool CompressionReader::GetHeader(string &v_header)
{

    if (!vcf_hdr)
        return false;
    kstring_t str = {0, 0, nullptr};
    // bcf_hdr_remove(vcf_hdr,BCF_HL_INFO, NULL);
    bcf_hdr_format(vcf_hdr, 0, &str);
    char *ptr = strstr(str.s, "#CHROM");
    v_header.assign(str.s, ptr - str.s);
    if (str.m)
        free(str.s);
    return true;
}

// ***************************************************************************************************************************************
void CompressionReader::GetWhereChrom(vector<pair<std::string, uint32_t>> &_where_chrom, vector<int64_t> &_chunks_min_pos)
{
    _where_chrom = where_chrom;
    _chunks_min_pos = chunks_min_pos;
}
// ***************************************************************************************************************************************
vector<uint32_t> CompressionReader::GetActualVariants()
{
    return actual_variants;
}
// ***************************************************************************************************************************************
bool CompressionReader::setBitVector()
{
    if (no_samples == 0)
        return false;
    vec_len = (no_samples * ploidy) / 8 + (((no_samples * ploidy) % 8) ? 1 : 0);
    block_max_size = vec_len * no_vec_in_block + 1;
    // phased_block_max_size = ((no_samples *  (ploidy-1)) / 8 + (((no_samples *  (ploidy-1)) % 8)?1:0))*no_vec_in_block/2;
    bv.Create(block_max_size);

    vec_read_fixed_fields = 0;
    fixed_fields_id = 0;
    block_id = 0;
    vec_read_in_block = 0;
    return true;
}
// ***************************************************************************************************************************************
void CompressionReader::GetOtherField(vector<key_desc> &_keys, uint32_t &_no_keys, int &_key_gt_id)
{
    _keys = keys;
    _no_keys = no_keys;
    _key_gt_id = key_gt_id;
}
void CompressionReader::UpdateKeys(vector<key_desc> &_keys)
{
    // vector<key_desc>  tmp_keys = _keys;
    size_t k = 0;
    if (order.size() < no_keys - no_flt_keys)
    {
        for (size_t i = 0; i < _keys.size(); i++)
        {
            if (_keys[i].keys_type == key_type_t::info)
            {

                auto it = std::find(order.begin(), order.end(), _keys[i].actual_field_id);
                if (it != order.end())
                {
                    _keys[i].actual_field_id = order[k++];
                }
            }
            else if (_keys[i].keys_type == key_type_t::fmt)
            {
                auto it = std::find(order.begin(), order.end(), _keys[i].actual_field_id);
                if (it != order.end())
                {
                    _keys[i].actual_field_id = order[k++];
                }
            }
        }
    }
    else
    {
        for (size_t i = 0; i < _keys.size(); i++)
        {
            if (_keys[i].keys_type == key_type_t::info)
            {

                _keys[i].actual_field_id = order[k++];
            }
            else if (_keys[i].keys_type == key_type_t::fmt)
            {
                _keys[i].actual_field_id = order[k++];
            }
        }
    }
}
// ***************************************************************************************************************************************
void CompressionReader::InitVarinats(File_Handle_2 *_file_handle2)
{
    file_handle2 = _file_handle2;
    GetFilterInfoFormatKeys(no_flt_keys, no_info_keys, no_fmt_keys, keys);
    no_keys = no_flt_keys + no_info_keys + no_fmt_keys;
    v_o_buf.resize(no_keys);    // 根据其他字段的数量初始化缓冲v_o_buf
    for (uint32_t i = 0; i < no_keys; i++)
        v_o_buf[i].SetMaxSize(max_buffer_size, 0);  // max_buffer_size = 8M(8 << 20)
    v_buf_ids_size.resize(no_keys, -1);
    v_buf_ids_data.resize(no_keys, -1);
    // std::cerr << "no_keys:" << no_keys << endl;
    
    // 构建索引，确认字段标识符与其具体位置之间的映射关系（将key_id映射到数组中的索引值）
    for (uint32_t i = 0; i < no_keys; i++)
    {
        switch (keys[i].keys_type)
        {
        case key_type_t::flt:
            if (keys[i].key_id >= FilterIdToFieldId.size())
                FilterIdToFieldId.resize(keys[i].key_id + 1, -1);
            FilterIdToFieldId[keys[i].key_id] = i;
            break;
        case key_type_t::info:
            // std::cerr << "no_samples:" <<keys[i].key_id << endl;
            if (keys[i].key_id >= InfoIdToFieldId.size())
                InfoIdToFieldId.resize(keys[i].key_id + 1, -1);
            InfoIdToFieldId[keys[i].key_id] = i;
            break;
        case key_type_t::fmt:
            if (keys[i].key_id >= FormatIdToFieldId.size())
                FormatIdToFieldId.resize(keys[i].key_id + 1, -1);
            FormatIdToFieldId[keys[i].key_id] = i;
            // std::cerr<<"InfoIdToFieldId:"<<FormatIdToFieldId[keys[i].key_id]<<endl;
            break;
        }
    }

    // file_handle2->RegisterStream("start");
    for (uint32_t i = 0; i < no_keys; i++)
    {
        v_buf_ids_size[i] = file_handle2->RegisterStream("key_" + to_string(i) + "_size");
        // std::cerr<<v_buf_ids_size[i]<<endl;
    }
    for (uint32_t i = 0; i < no_keys; i++)
    {
        v_buf_ids_data[i] = file_handle2->RegisterStream("key_" + to_string(i) + "_data");
        // std::cerr<<v_buf_ids_data[i]<<endl;
    }
}
// ***************************************************************************************************************************************
// 从元信息行中获取Filter/Info/Format字段中子字段的数量和对应的keys表
// no_flt_keys: Filter包含子字段的数量
// no_info_keys: Info包含子字段的数量
// no_fmt_keys: Format包含子字段的数量
bool CompressionReader::GetFilterInfoFormatKeys(int &no_flt_keys, int &no_info_keys, int &no_fmt_keys, vector<key_desc> &keys)
{
    if (!vcf_hdr)
        return false;

    no_flt_keys = 0;
    no_info_keys = 0;
    no_fmt_keys = 0;

    key_desc new_key;
    // std::cerr<<vcf_hdr->nhrec<<endl;
    for (int i = 0; i < vcf_hdr->nhrec; i++)
    {

        if (vcf_hdr->hrec[i]->type == BCF_HL_FLT || vcf_hdr->hrec[i]->type == BCF_HL_INFO || vcf_hdr->hrec[i]->type == BCF_HL_FMT)
        {
            // checking if it is a duplicate; if so, curr_dict_id different (and not increased)
            int id = bcf_hdr_id2int(vcf_hdr, BCF_DT_ID, vcf_hdr->hrec[i]->vals[0]);     // 通过哈希表快速查找给定的id对应的内部整数标识符

            new_key.key_id = id;
            if (vcf_hdr->hrec[i]->type == BCF_HL_FLT)
            {
                no_flt_keys++;
                new_key.keys_type = key_type_t::flt;
                new_key.type = BCF_HT_FLAG;
            }
            else if (vcf_hdr->hrec[i]->type == BCF_HL_INFO || vcf_hdr->hrec[i]->type == BCF_HL_FMT)
            {
                if (vcf_hdr->hrec[i]->type == BCF_HL_FMT) // FORMAT
                {
                    no_fmt_keys++;
                    new_key.keys_type = key_type_t::fmt;
                    new_key.type = bcf_hdr_id2type(vcf_hdr, BCF_HL_FMT, id);
                    if (strcmp(vcf_hdr->id[BCF_DT_ID][id].key, "GT") == 0)
                    {
                        key_gt_id = (int)keys.size();
                    }
                }
                else // INFO
                {
                    no_info_keys++;
                    new_key.keys_type = key_type_t::info;
                    new_key.type = bcf_hdr_id2type(vcf_hdr, BCF_HL_INFO, id);
                }
            }
            new_key.actual_field_id = (uint32_t)keys.size();
            // std::cerr<<"new_key.actual_field_id:"<<new_key.key_id<<":"<<new_key.actual_field_id<<endl;
            keys.emplace_back(new_key);
        }
    }
    keys.shrink_to_fit();   // 通过释放多余的内存来减少内存占用
    return true;
}
// ************************************************************************************
// 读取每个变体行,并获取其它字段中子字段的数据码流
// fields:存放每个数据码流,其数据结构如下:
    // {
        //存放其它字段中子字段的数据流
        // present: 是否在元信息行中
        // data: 实际数据流
        // data_size ： 数据流大小
        // typedef struct field_desc_tag {
        //     bool present = false;  // true if present in description
        //     char *data = nullptr;
        //     uint32_t data_size = 0; //current size of allocated memory
        // ......
        // }
    // }
bool CompressionReader::GetVariantFromRec(bcf1_t *rec, vector<field_desc> &fields)
{
    vector<int> field_order;
    field_order.reserve(no_keys);
    if (rec->d.n_flt)
    {

        for (int i = 0; i < rec->d.n_flt; ++i)
        {
            fields[FilterIdToFieldId[rec->d.flt[i]]].present = true; // DEV, TMP
        }
    }

    int curr_size = 0;

    // INFO
    if (rec->n_info)
    {
        for (int i = 0; i < rec->n_info; ++i)
        {

            bcf_info_t *z = &rec->d.info[i];
            // if(order.size() < no_keys - no_flt_keys || field_order_flag)
            field_order.emplace_back(InfoIdToFieldId[z->key]);
            auto &cur_field = fields[InfoIdToFieldId[z->key]];
            cur_field.present = true;
            if (!z->vptr)
                continue;
            if (z->key >= vcf_hdr->n[BCF_DT_ID])
            {
                hts_log_error("Invalid BCF, the INFO index is too large");
                return -1;
            }
            if (z->len <= 0)
                continue;

            int type = bcf_hdr_id2type(vcf_hdr, BCF_HL_INFO, z->key);
            int curr_size = 0;

            switch (type)
            {
            case BCF_HT_INT:

                curr_size = bcf_get_info_values(vcf_hdr, rec, vcf_hdr->id[BCF_DT_ID][z->key].key, &dst_int, &ndst_int, type);
                break;
            case BCF_HT_REAL:

                curr_size = bcf_get_info_values(vcf_hdr, rec, vcf_hdr->id[BCF_DT_ID][z->key].key, &dst_real, &ndst_real, type);
                break;
            case BCF_HT_STR:

                curr_size = bcf_get_info_values(vcf_hdr, rec, vcf_hdr->id[BCF_DT_ID][z->key].key, &dst_str, &ndst_str, type);
                break;
            case BCF_HT_FLAG:
                curr_size = bcf_get_info_values(vcf_hdr, rec, vcf_hdr->id[BCF_DT_ID][z->key].key, &dst_flag, &ndst_flag, type);
                break;
            }

            if (curr_size)
            {
                switch (type)
                {
                case BCF_HT_INT:
                    cur_field.data_size = (uint32_t)curr_size * 4;
                    cur_field.data = new char[cur_field.data_size];
                    memcpy(cur_field.data, (char *)dst_int, cur_field.data_size);
                    break;
                case BCF_HT_REAL:
                    cur_field.data_size = (uint32_t)curr_size * 4;
                    cur_field.data = new char[cur_field.data_size];
                    memcpy(cur_field.data, (char *)dst_real, cur_field.data_size);
                    break;
                case BCF_HT_STR:
                    cur_field.data_size = curr_size;
                    cur_field.data = new char[cur_field.data_size];
                    memcpy(cur_field.data, (char *)dst_str, cur_field.data_size);
                    break;
                case BCF_HT_FLAG:
                    cur_field.data = nullptr;
                    cur_field.data_size = 0;
                    break;
                }
            }
            else
            {
                std::cerr << "Error getting variant" << endl;
                return false;
            }
        }
    }

    // FORMAT and individual information
    if (rec->n_sample)
    {

        int i; // , j;
        if (rec->n_fmt)
        {

            bcf_fmt_t *fmt = rec->d.fmt;
            for (i = 0; i < (int)rec->n_fmt; ++i)
            {

                if (!fmt[i].p)
                    continue;
                if (fmt[i].id < 0) //! bcf_hdr_idinfo_exists(h,BCF_HL_FMT,fmt[i].id) )
                {
                    hts_log_error("Invalid BCF, the FORMAT tag id=%d not present in the header", fmt[i].id);
                    abort();
                }

                // if(order.size() < no_keys - no_flt_keys || field_order_flag)

                field_order.emplace_back(FormatIdToFieldId[fmt[i].id]);
                auto &cur_field = fields[FormatIdToFieldId[fmt[i].id]];
                cur_field.present = true;

                int bcf_ht_type = bcf_hdr_id2type(vcf_hdr, BCF_HL_FMT, fmt[i].id); // BCF_HT_INT or BCF_HT_REAL or BCF_HT_FLAG or BCF_HT_STR
                auto vcf_hdr_key = vcf_hdr->id[BCF_DT_ID][fmt[i].id].key;
                // std::cerr<<vcf_hdr_key<<endl;
                if (strcmp(vcf_hdr_key, "GT") == 0)
                {
                    int *gt_arr = NULL, ngt_arr = 0;
                    curr_size = bcf_get_genotypes(vcf_hdr, rec, &gt_arr, &ngt_arr);
                    cur_field.data_size = curr_size - rec->n_sample;    // 这样做的目的？
                    cur_field.data = new char[cur_field.data_size];
                    int cur_phased_pos = 0;
                    for (uint32_t j = 0; j < rec->n_sample; ++j)
                    {
                        for (uint32_t k = 1; k < ploidy; ++k)
                        {
                            if (bcf_gt_is_phased(gt_arr[j * ploidy + k]))
                                if (gt_arr[j * ploidy + k] == GT_NOT_CALL)
                                    cur_field.data[cur_phased_pos++] = '.';
                                else
                                    cur_field.data[cur_phased_pos++] = '|';
                            else
                                cur_field.data[cur_phased_pos++] = '/';
                        }
                    }
                    free(gt_arr);
                    continue;
                }
                else
                {
                    switch (bcf_ht_type)
                    {
                    case BCF_HT_INT:
                        curr_size = bcf_get_format_values(vcf_hdr, rec, vcf_hdr_key, &dst_int, &ndst_int, bcf_ht_type);
                        break;
                    case BCF_HT_REAL:
                        curr_size = bcf_get_format_values(vcf_hdr, rec, vcf_hdr_key, &dst_real, &ndst_real, bcf_ht_type);
                        break;
                    case BCF_HT_STR:
                        curr_size = bcf_get_format_values(vcf_hdr, rec, vcf_hdr_key, &dst_str, &ndst_str, bcf_ht_type);
                        break;
                    case BCF_HT_FLAG:
                        curr_size = bcf_get_format_values(vcf_hdr, rec, vcf_hdr_key, &dst_flag, &ndst_flag, bcf_ht_type);
                        break;
                    }
                }

                if (curr_size)
                {
                    // cur_field.data_size = (uint32_t)curr_size;

                    if (bcf_ht_type == BCF_HT_INT || bcf_ht_type == BCF_HT_REAL)
                    {
                        cur_field.data_size = (uint32_t)curr_size * 4;
                        cur_field.data = new char[cur_field.data_size];

                        if (bcf_ht_type == BCF_HT_INT)
                        {
                            memcpy(cur_field.data, dst_int, cur_field.data_size);
                        }
                        else if (bcf_ht_type == BCF_HT_REAL)
                            memcpy(cur_field.data, dst_real, cur_field.data_size);
                        // else if (bcf_ht_type == BCF_HT_STR)
                        //     memcpy(cur_field.data, dst_int, curr_size * 4); // GTs are ints!
                    }
                    else if (bcf_ht_type == BCF_HT_STR)
                    {
                        cur_field.data_size = (uint32_t)curr_size;
                        cur_field.data = new char[cur_field.data_size];
                        memcpy(cur_field.data, dst_str, cur_field.data_size);
                    }
                    else
                        assert(0);
                }
            }
        }
    }
    // if(order.size() < no_keys - no_flt_keys || field_order_flag){
    //     field_order_flag =  false;
    // 拓扑排序
    for (size_t i = 0; i < field_order.size() - 1; ++i)
    {
        // std::cerr<<field_order[i]<<" ";
        if (field_order_graph[field_order[i]].find(field_order[i + 1]) == field_order_graph[field_order[i]].end())
        {
            field_order_graph[field_order[i]].insert(field_order[i + 1]);
            inDegree[field_order[i + 1]]++;
        }
        if (inDegree.find(field_order[i]) == inDegree.end())
        {

            inDegree[field_order[i]] = 0;
        }
    }
    // std::cerr<<endl;
    // order = topo_sort(field_order_graph,inDegree);

    // }

    return true;
}

// *******************************************************************************************************************************
bool CompressionReader::SetVariantOtherFields(vector<field_desc> &fields)
{
    for (uint32_t i = 0; i < no_keys; i++)
    {

        switch (keys[i].type)
        {
        case BCF_HT_INT:

            v_o_buf[i].WriteInt(fields[i].data, fields[i].present ? fields[i].data_size : 0);

#ifdef LOG_INFO
            {
                uint32_t *pp = (uint32_t *)fields[i].data;
                for (int j = 0; j < fields[i].data_size; ++j)
                    distinct_values[i].insert(pp[j]);
            }
#endif

            break;
        case BCF_HT_REAL:
            v_o_buf[i].WriteReal(fields[i].data, fields[i].present ? fields[i].data_size : 0);

#ifdef LOG_INFO
            {
                uint32_t *pp = (uint32_t *)fields[i].data;

                for (int j = 0; j < fields[i].data_size; ++j)
                    distinct_values[i].insert(pp[j]);
            }
#endif

            break;
        case BCF_HT_STR:
            v_o_buf[i].WriteText(fields[i].data, fields[i].present ? fields[i].data_size : 0);
            break;
        case BCF_HT_FLAG:

            v_o_buf[i].WriteFlag(fields[i].present);
            break;
        }

        if (v_o_buf[i].IsFull())
        {
            // std::cerr<<"v_buf_ids_size[i]:"<<v_buf_ids_size[i]<<endl;
            // std::cerr<<"v_buf_ids_size[i]:"<<v_buf_ids_size[i]<<endl;
            auto part_id = file_handle2->AddPartPrepare(v_buf_ids_size[i]);
            file_handle2->AddPartPrepare(v_buf_ids_data[i]);
            // std::cerr<<part_id<<endl;
            vector<uint32_t> v_size;
            vector<uint8_t> v_data;

            v_o_buf[i].GetBuffer(v_size, v_data);

            SPackage pck(i, v_buf_ids_size[i], v_buf_ids_data[i], part_id, v_size, v_data);
            // cout<< "其他字段数据进入队列：key_id: "<<i<<endl;
            part_queue->Push(pck);
        }
    }

    return true;
}
// ***************************************************************************************************************************************
// Splits multiple alleles sites, reads genotypes, creates blocks of bytes to process, fills out [archive_name].bcf file

// 对VCF/BCF进行读取并进行处理,主要包括对其它字段, 固定字段和基因型的处理;以及在变体行中的子字段实际顺序的获取
// setBitVector 设置基因型比特矩阵的相关参数
// bcf_read1 htslib库中用于读取变体行的函数
bool CompressionReader::ProcessInVCF()
{

    if (!vcf_hdr_read)
        ReadFile();
    setBitVector();
    no_vec = 0;
    tmpi = 0;
    temp = 0;
    cur_pos = 0;
    gt_data = new int[no_samples * ploidy];  
    no_actual_variants = 0;
    int file_nums = 1;
    if (merge_flag)
    {
        file_nums = merge_files.size();
    }

    for (int i = 0; i < file_nums; i++)
    {
        if (merge_flag)
            in_file = merge_files[i];

        while (bcf_read1(in_file, vcf_hdr, vcf_record) >= 0)  // 从内存中解析数据，不会涉及磁盘 I/O 操作(HTSlib 内部使用了批量读取和缓存机制,在OpenForReading中实现)
        {
            // std::cerr<<"no_samples:"<<no_samples<<endl;
            variant_desc_t desc;
            if (vcf_record->errcode)
            {
                std::cerr << "Repair VCF file\n";
                exit(9);
            }
            bcf_unpack(vcf_record, BCF_UN_ALL);  // 解包VCF记录
            if (vcf_record->d.fmt->n != (int)ploidy)
            {
                std::cerr << "Wrong ploidy (not equal to " << ploidy << ") for record at position " << vcf_record->pos << ".\n";
                std::cerr << "Repair VCF file OR set correct ploidy using -p option\n";
                exit(9);
            }
            if (tmpi % 100000 == 0)    // 每处理10万条记录时，程序会输出当前处理到的记录数
            {
                std::cerr << tmpi << "\r";
                fflush(stdout);
            }
            if (compress_mode == compress_mode_t::lossless_mode)
            {

                std::vector<field_desc> curr_field(keys.size());

                GetVariantFromRec(vcf_record, curr_field);  // 读取每个变体行,并获取其它字段中子字段的数据码流

                SetVariantOtherFields(curr_field);  // // 将其它字段数据码流fields按不同数据类型编码存放到v_o_buf缓存中

                // 临时数据析构
                for (size_t j = 0; j < keys.size(); ++j)
                {
                    if (curr_field[j].data_size > 0)
                    {
                        if (curr_field[j].data)
                            delete[] curr_field[j].data;
                        curr_field[j].data = nullptr;
                        curr_field[j].data_size = 0;
                    }
                }
                curr_field.clear();
            }

            ++no_actual_variants;
            ProcessFixedVariants(vcf_record, desc);  // 处理变体中的固定字段和基因型，主要包括对变体进行拆分和对基因型进行初始的比特编码

            tmpi++;
        }
    }
    if (compress_mode == compress_mode_t::lossless_mode)
        order = topo_sort(field_order_graph, inDegree);
    if (cur_g_data)
    {
        delete[] cur_g_data;
    }
    if (gt_data)
    {
        delete[] gt_data;
    }

    std::cerr << "Read all the variants and gentotypes" << endl;
    // Last pack (may be smaller than block size）

    CloseFiles();
    return true;
}
// void CompressionReader::addPhased(int * gt_data, int ngt_data){

//     for(int i = 0;i<ngt_data;i++)
//         if(bcf_gt_is_phased(gt_data[i&1]) )
//         {

//         }
//         else {
//             std::cerr<<cur_g_data[i]<<" ";
//         }

// }
// ***************************************************************************************************************************************

// 后续处理多倍型混合可以在这里入手
void CompressionReader::ProcessFixedVariants(bcf1_t *vcf_record, variant_desc_t &desc)
{

    if (vcf_record->n_allele <= 2)
    {
        desc.ref.erase(); // REF
        if (vcf_record->n_allele > 0)
        {
            if (vcf_record->pos + 1 == cur_pos)
            {

                desc.ref = to_string(++temp) + vcf_record->d.allele[0];  // 位置相同则使用递增编号
            }
            else
            {
                temp = 0;
                desc.ref = vcf_record->d.allele[0];
            }
        }
        else
            desc.ref = '.';
        if (vcf_record->n_allele > 1)
        {
            desc.alt.erase(); // ALT
            desc.alt += vcf_record->d.allele[1];
        }

        bcf_get_genotypes(vcf_hdr, vcf_record, &cur_g_data, &ncur_g_data);   // 获取基因型数据，ncur_g_data是10，可以从这分块？

        for (int i = 0; i < ncur_g_data; i++)
        {
            gt_data[i] = bcf_gt_allele(cur_g_data[i]);  // 将基因型数据转化为等位基因值
        }

        addVariant(gt_data, ncur_g_data, desc);
        bcf_clear(vcf_record);
    }
    else
    {
        //  三等位基因情况，带 <M> 或 <N> 的处理
        if (vcf_record->n_allele == 3 && (strcmp(vcf_record->d.allele[2], "<M>") == 0 || strcmp(vcf_record->d.allele[2], "<N>") == 0))
        {

            desc.ref.erase(); // REF
            if (vcf_record->n_allele > 0)
            {
                if (vcf_record->pos + 1 == cur_pos)
                {
                    ;
                }
                else
                {
                    temp = 0;
                }
                desc.ref = to_string(++temp) + vcf_record->d.allele[0];
                // snprintf(temp_str, sizeof(temp_str), "%d", ++temp);
                // desc.ref = temp_str;
                // desc.ref += vcf_record->d.allele[0];
            }
            else
                desc.ref = '.';
            desc.alt.erase(); // ALT
            desc.alt += vcf_record->d.allele[1];
            desc.alt += ',';
            desc.alt += vcf_record->d.allele[2];
            bcf_get_genotypes(vcf_hdr, vcf_record, &cur_g_data, &ncur_g_data);
            // if (gt_data)
            //     delete[] gt_data;
            // gt_data = new int[ncur_g_data];
            for (int i = 0; i < ncur_g_data; i++)
            {
                gt_data[i] = bcf_gt_allele(cur_g_data[i]);
            }

            addVariant(gt_data, ncur_g_data, desc);
            bcf_clear(vcf_record);
        }
        else
        {

            if (vcf_record->pos + 1 != cur_pos)
                temp = 0;
            for (int n = 1; n < vcf_record->n_allele; n++) // create one line for each single allele
            {

                // size_t len_ref = strlen(vcf_record->d.allele[0]), len_alt = strlen(vcf_record->d.allele[n]);
                // if (len_ref > 1 && len_alt > 1)
                // {
                //     while ((vcf_record->d.allele[0][len_ref - 1] == vcf_record->d.allele[n][len_alt - 1]) && len_ref > 1 && len_alt > 1)
                //     {
                //         len_ref--;
                //         len_alt--;
                //     }
                // }

                desc.ref.erase(); // REF

                if (vcf_record->n_allele > 0)
                {
                    desc.ref += vcf_record->d.allele[0];
                    // desc.ref = to_string(++temp) + desc.ref.substr(0, len_ref);
                    desc.ref = to_string(++temp) + desc.ref;
                }
                else
                    desc.ref = '.';
                desc.alt.erase(); // ALT
                desc.alt += vcf_record->d.allele[n];
                // desc.alt = desc.alt.substr(0, len_alt);
                if (n == 1)
                    desc.alt += ",<N>";
                else
                    desc.alt += ",<M>";

                bcf_get_genotypes(vcf_hdr, vcf_record, &cur_g_data, &ncur_g_data);
                // if (gt_data)
                //     delete[] gt_data;
                // gt_data = new int[ncur_g_data];
                // fill_n(gt_data, ncur_g_data, 0);
                for (int i = 0; i < ncur_g_data; i++)
                { // gt_arr needed to create bit vectors
                    // std::cerr<<bcf_gt_allele(cur_g_data[i])<<" ";
                    if (bcf_gt_allele(cur_g_data[i]) != 0)
                    {
                        if (bcf_gt_allele(cur_g_data[i]) == n)
                            cur_g_data[i] = bcf_gt_phased(1);
                        else if (bcf_gt_is_missing(cur_g_data[i]) || cur_g_data[i] == GT_NOT_CALL)
                            cur_g_data[i] = bcf_gt_missing;
                        else
                            cur_g_data[i] = bcf_gt_phased(2);
                    }

                    gt_data[i] = bcf_gt_allele(cur_g_data[i]);
                }
                addVariant(gt_data, ncur_g_data, desc);
            }
            tmpi += vcf_record->n_allele - 1;
            // temp_count--;
            bcf_clear(vcf_record);
        }
    }
    cur_pos = desc.pos;
}
// ***************************************************************************************************************************************
// 将固定字段和基因型数据码流添加到对应的缓存中，并分块放入Gt_queue队列中
// 从这里来操作细分块逻辑（按行读取方式不变，推入队列方式采用多线程）
#include <iostream>
#include <bitset>
#include <cstdint>
using namespace std;
void CompressionReader::addVariant(int *gt_data, int ngt_data, variant_desc_t &desc)
{

    desc.chrom = vcf_hdr->id[BCF_DT_CTG][vcf_record->rid].key; // CHROM

    desc.pos = vcf_record->pos + 1;                      // POS
    desc.id = vcf_record->d.id ? vcf_record->d.id : "."; // ID
                                                         //    desc.qual.erase(); // QUAL
    if (bcf_float_is_missing(vcf_record->qual))
        desc.qual = ".";
    else
    {
        kstring_t s = {0, 0, 0};
        kputd(vcf_record->qual, &s);
        //      desc.qual += s.s;
        desc.qual = s.s;    // QUAL为什么要存成字符？
        free(s.s);
    }
    if (!vec_read_fixed_fields)
        chunks_min_pos.emplace_back(desc.pos);

    if (start_flag)
    {
        cur_chrom = desc.chrom;
        start_flag = false;
    }

    // 编码基因型数据（根据染色体是否发生切换执行不同的逻辑）
    if (desc.chrom == cur_chrom)
    {
        // for (int i = 0; i < ngt_data; i++)
        // {
        //     printf("%d ", gt_data[i]);
        // }
        // printf("\n");

        // 编码第1位
        for (int i = 0; i < ngt_data; i++)
        {
            if (gt_data[i] == 0 || gt_data[i] == 1)
            {
                bv.PutBit(0);   // PutBit 函数逐位将数据放入一个缓冲区 word_buffer，一旦缓冲区填满，它会调用 PutWord 来处理完整的 32 位数据。PutWord 函数负责将 32 位的数据拆分为 4 个字节，并逐个字节写出。
            }
            else // if(bcf_gt_is_missing(gt_arr[i]) || bcf_gt_allele(gt_arr[i]) == 2)
            {
                bv.PutBit(1);
            }
        }

        bv.FlushPartialWordBuffer();    // 将部分填充的 word_buffer 进行对齐，并将其中的字节按需输出

        // Set vector with less significant bits of dibits
        for (int i = 0; i < ngt_data; i++)
        {

            if (gt_data[i] == 1 || gt_data[i] == 2)
            {
                bv.PutBit(1);
            }
            else // 0
            {
                bv.PutBit(0);
            }
        }
        bv.FlushPartialWordBuffer();

        v_vcf_data_compress.emplace_back(desc);  // variant_desc_t
        vec_read_fixed_fields++;
        if (vec_read_fixed_fields == no_fixed_fields)  // 读取的固定字段数量达到指定数量no_fixed_fields，存入actual_variants中
        {
            actual_variants.emplace_back(no_actual_variants);
            no_actual_variants = 0;
            vec_read_fixed_fields = 0;
        }
        vec_read_in_block += 2; // Two vectors added

        // bv一致循环构造矩阵块，直到达到行数条件
        if (vec_read_in_block == no_vec_in_block) // Insert complete block into queue of blocks
        {

            bv.TakeOwnership();

            // cout<< "=======达到块数条件=========\n";
            // for (size_t i = 0; i < 40; ++i) {
            //     std::cout << "bv.mem_buffer[" << i << "] (in binary) = " << std::bitset<8>(bv.mem_buffer[i]) << std::endl;
            // }

            Gt_queue->Push(block_id, bv.mem_buffer, vec_read_in_block, v_vcf_data_compress); 
            v_vcf_data_compress.clear();
            no_chrom_num++;
            no_vec = no_vec + vec_read_in_block;
            block_id++; // 更新块 ID
            bv.Close();
            bv.Create(block_max_size);
            vec_read_in_block = 0;
        }
        no_chrom = cur_chrom;
    }
    else    // 当前变体的染色体与前一个染色体不同,防止下一个染色体的数据就会混入到当前染色体的块中
    {
        if (vec_read_fixed_fields)
        {
            actual_variants.emplace_back(no_actual_variants - 1);
            no_actual_variants = 1;
            vec_read_fixed_fields = 0;
        }
        if (!vec_read_fixed_fields)
            chunks_min_pos.emplace_back(desc.pos);
        vec_read_fixed_fields++;
        cur_chrom = desc.chrom;
        if (vec_read_in_block)
        {
            bv.TakeOwnership();
            Gt_queue->Push(block_id, bv.mem_buffer, vec_read_in_block, v_vcf_data_compress);
            v_vcf_data_compress.clear();
            no_chrom_num++;
            no_vec = no_vec + vec_read_in_block;
            block_id++;
            bv.Close();
            bv.Create(block_max_size);
            vec_read_in_block = 0;
        }
        for (int i = 0; i < ngt_data; i++)
        {

            if (gt_data[i] == 0 || gt_data[i] == 1)
            {
                bv.PutBit(0);
            }
            else // if(bcf_gt_is_missing(gt_arr[i]) || bcf_gt_allele(gt_arr[i]) == 2)
            {
                bv.PutBit(1);
            }
        }
        bv.FlushPartialWordBuffer();
        // Set vector with less significant bits of dibits
        for (int i = 0; i < ngt_data; i++)
        {
            if (gt_data[i] == 1 || gt_data[i] == 2)
            {
                bv.PutBit(1);
            }
            else // 0
            {
                bv.PutBit(0);
            }
        }
        bv.FlushPartialWordBuffer();

        vec_read_in_block += 2; // Two vectors added
        v_vcf_data_compress.emplace_back(desc);
        where_chrom.emplace_back(make_pair(no_chrom, no_chrom_num));
    }
}

// ***************************************************************************************************************************************
uint32_t CompressionReader::setNoVecBlock(GSC_Params &params)
{
        params.var_in_block = no_samples * params.ploidy;

        int numThreads = std::thread::hardware_concurrency() / 2;

        int numChunks = 1 + (params.var_in_block / 1024);   // 取商而忽略小数部分

        if (numChunks < numThreads)
        {
            numThreads = numChunks;
        }

        params.no_gt_threads = numThreads;
        // params.no_gt_threads = 1;

        if (params.var_in_block < 1024)
        {
            chunk_size = CHUNK_SIZE1;
        }
        else if (params.var_in_block < 4096)
        {
            chunk_size = CHUNK_SIZE2;
        }
        else if (params.var_in_block < 8192)
        {
            chunk_size = CHUNK_SIZE3;
        }
        else
        {
            chunk_size = params.var_in_block;
        }
        params.no_blocks = chunk_size / params.var_in_block;

        no_fixed_fields = params.no_blocks * params.var_in_block;

        if (params.task_mode == task_mode_t::mcompress)
        {
            no_vec_in_block = params.var_in_block * 2;      // 单个向量（全零 or 复制）
            params.vec_len = params.var_in_block / 8 + ((params.var_in_block % 8) ? 1 : 0);  // 矩阵每行占用的字节数
            params.n_samples = no_samples;
        }
    return 0;
}
void CompressionReader::CloseFiles()
{
    // std::cerr<<"Closing files"<<endl;
    if (vec_read_fixed_fields)
    {
        actual_variants.emplace_back(no_actual_variants);
        no_actual_variants = 0;
        vec_read_fixed_fields = 0;
    }
    if (vec_read_in_block)
    {

        bv.TakeOwnership();
        // cout<< "=======关闭文件=========\n";
        // for (size_t i = 0; i < 12; ++i) {                
        //     std::cout << "bv.mem_buffer[" << i << "] (in binary) = " << std::bitset<8>(bv.mem_buffer[i]) << std::endl;
        // }
        // Gt_queue->Push(block_id, bv.mem_buffer, vec_read_in_block,v_vcf_data_compress,chrom_flag::none);
        Gt_queue->Push(block_id, bv.mem_buffer, vec_read_in_block, v_vcf_data_compress);
        // v_vcf_data_compress.clear();
        no_chrom_num++;
        block_id++;
        no_vec = no_vec + vec_read_in_block;
        vec_read_in_block = 0;
        bv.Close();
    }

    v_vcf_data_compress.emplace_back(variant_desc_t());
    Gt_queue->Push(block_id, nullptr, 0, v_vcf_data_compress);

    block_id = 0;
    Gt_queue->Complete();

    where_chrom.emplace_back(make_pair(no_chrom, no_chrom_num));

    no_chrom_num = 0;
    if (compress_mode == compress_mode_t::lossless_mode)
    {
        for (uint32_t i = 0; i < no_keys; ++i)
        {

            v_o_buf[i].GetBuffer(v_size, v_data);

            auto part_id = file_handle2->AddPartPrepare(v_buf_ids_size[i]);
            file_handle2->AddPartPrepare(v_buf_ids_data[i]);

            SPackage pck(i, v_buf_ids_size[i], v_buf_ids_data[i], part_id, v_size, v_data);

            part_queue->Push(pck);
        }

        part_queue->Complete();
    }
    // file_handle2->Close();
}
// ***************************************************************************************************************************************
vector<int> CompressionReader::topo_sort(unordered_map<int, unordered_set<int>> &graph, unordered_map<int, int> inDegree)
{

    vector<int> result;
    queue<int> q;
    for (const auto &p : inDegree)
    {
        if (p.second == 0)
        {
            q.push(p.first);
        }
    }
    while (!q.empty())
    {
        if (q.size() > 1)
            field_order_flag = true;
        int cur = q.front();
        q.pop();
        result.push_back(cur);
        for (int next : graph[cur])
        {

            if (--inDegree[next] == 0)
            {
                q.push(next);
            }
        }
    }

    // Step 3: Check if there's a cycle in the graph
    if (result.size() != graph.size())
    {
        return vector<int>();
    }

    return result;
}