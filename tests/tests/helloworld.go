package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

func main() {
	fmt.Println("Hello, World!")
	fmt.Println("你好，世界！")
	if len(os.Args) > 1 {
		fmt.Println("Arguments passed:", os.Args[1:])
	}

	dir, err := filepath.Abs(filepath.Dir(os.Args[0]))
	if err != nil {
		fmt.Println("Error getting directory:", err)
		return
	}

	filePath := filepath.Join(dir, "helloworld.txt")
	file, err := os.Create(filePath)
	if err != nil {
		fmt.Println("Error creating file:", err)
		return
	}
	defer file.Close()

	writeFile := func(s string) {
		if err != nil {
			return
		}
		_, err = file.WriteString(s + "\n")
	}
	writeFile("Hello, World!")
	writeFile("你好，世界！")
	writeFile(strings.Join(os.Args[1:], " "))
	if err != nil {
		fmt.Println("Error writing to file:", err)
		return
	}

	fmt.Println("helloworld.txt has been written in", dir)
}
