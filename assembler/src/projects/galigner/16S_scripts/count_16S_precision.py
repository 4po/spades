__author__ = 'tanunia'

import sys
import multiprocessing as mp

from Bio import Entrez

def get_taxid(species):
    """to get data from ncbi taxomomy, we need to have the taxid.  we can
    get that by passing the species name to esearch, which will return
    the tax id"""
    species = species.replace(" ", "+").strip()
    search = Entrez.esearch(term = species, db = "taxonomy", retmode = "xml")
    record = Entrez.read(search)
    if record['Count'] == '1':
        return record['IdList'][0]
    else:
        print "No such species: ", species
        return None

def get_tax_data(taxid):
    """once we have the taxid, we can fetch the record"""
    search = Entrez.efetch(id = taxid, db = "taxonomy", retmode = "xml")
    return Entrez.read(search)

def save(orgs, filename, types):
    fout = open(filename, "w")
    ind = 0
    for org in orgs:
        if org != None:
            ind += 1
            save_tax_data(ind, org, fout, types)
    fout.close()

def save_tax_data(org, mp, f, types):
    s = str(org)
    for t in types:
        if t in mp.keys():
            s += '\t' + t + ":" + mp[t]
    s += '\n'
    f.write(s)

def get_lineage(id):
    taxid = get_taxid(id.split(";")[-1])
    if taxid == None:
        return None
    data = get_tax_data(taxid)
    #print data
    lineage = {d['Rank']:d['ScientificName'] for d in
        data[0]['LineageEx'] if d['Rank'] in types}
    lineage["genus"] =id.split(";")[-1]
    return lineage
    #print gene, lineage
    #save_tax_data(gene, lineage, file_save_data, types)

Entrez.email = "tanunia@gmail.com"
if not Entrez.email:
    print "you must add your email address"
    sys.exit(2)

types = ['superkingdom', 'kingdom', 'phylum', 'class', 'order', 'family', 'genus', 'species']

#print get_lineage("Alistipes")
#exit(-1)
file_with_16S = open(sys.argv[1], "r")
#file_save_data = open(sys.argv[3], "w")

rna16S = [x.strip() for x in file_with_16S.readlines()]
file_with_16S.close()
pool = mp.Pool(processes=16)
rna16S_lst = pool.map(get_lineage, rna16S)
print len(rna16S)
print len(rna16S_lst)
save(rna16S_lst, sys.argv[2], types)
exit(0)
#orgs_lst = pool.map(get_lineage, orgs)
#save(orgs_lst, "orgs_lineage.tsv", types)
#res_file = open("stats16S.tsv", "w")
