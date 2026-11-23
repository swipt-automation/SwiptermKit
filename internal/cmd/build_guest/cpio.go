package main

import (
	"fmt"
	"io"
)

const (
	cpioDirectory = 0o040000
	cpioRegular   = 0o100000
	cpioFirstNode = 721
)

type cpioEntry struct {
	name string
	mode uint32
	data []byte
}

func writeCPIO(writer io.Writer, entries []cpioEntry, mtime int64) error {
	for index, entry := range append(entries, cpioEntry{name: "TRAILER!!!"}) {
		node, links := uint32(cpioFirstNode+index), uint32(1)
		switch {
		case entry.mode == 0:
			node, mtime = 0, 0
		case entry.mode&cpioDirectory != 0:
			links = 2
		}
		header := fmt.Sprintf("070701%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%s\x00",
			node, entry.mode, 0, 0, links, mtime, len(entry.data), 0, 0, 0, 0, len(entry.name)+1, 0, entry.name)
		_, err := io.WriteString(writer, header+padding(len(header)))
		if err != nil {
			return err
		}
		_, err = writer.Write(entry.data)
		if err != nil {
			return err
		}
		_, err = io.WriteString(writer, padding(len(entry.data)))
		if err != nil {
			return err
		}
	}
	return nil
}

func padding(length int) string {
	return "\x00\x00\x00"[:(4-length%4)%4]
}
