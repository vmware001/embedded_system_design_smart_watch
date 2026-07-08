import fs from "node:fs";
import path from "node:path";
import {
  AlignmentType,
  Document,
  Footer,
  Header,
  HeadingLevel,
  ImportedXmlComponent,
  Packer,
  PageNumber,
  Paragraph,
  ShadingType,
  Table,
  TableCell,
  TableRow,
  TextRun,
  WidthType,
  convertInchesToTwip,
} from "docx";

const outputPath = process.argv[2];
if (!outputPath) {
  throw new Error("Usage: node create.js /absolute/path/output.docx");
}

const outputDir = path.dirname(outputPath);
const assetDir = path.join(outputDir, "assets");
fs.mkdirSync(assetDir, { recursive: true });

const T = String.raw;
const title = T`基于STM32F103C8T6的智能手表设计 — 项目计划书`;
const palette = {
  dark: "263238",
  primary: "37474F",
  light: "78909C",
  border: "D8E0E3",
  fill: "EEF3F6",
};

const font = { name: "Times New Roman", eastAsia: "SimSun" };

const run = (text, options = {}) =>
  new TextRun({ text, font, size: 24, ...options });

const para = (children, options = {}) =>
  new Paragraph({
    spacing: { after: 160, line: 300 },
    ...options,
    children: Array.isArray(children) ? children : [children],
  });

const bodyPara = (text, options = {}) =>
  para(run(text), {
    indent: { firstLine: convertInchesToTwip(0.33) },
    ...options,
  });

const heading = (text, level = 1) =>
  para(run(text, { bold: true, size: level === 1 ? 30 : 26, color: palette.dark }), {
    heading: level === 1 ? HeadingLevel.HEADING_1 : HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 120 },
  });

const cell = (text, options = {}) =>
  new TableCell({
    children: [para(run(text))],
    margins: { top: 120, bottom: 120, left: 120, right: 120 },
    ...options,
  });

const xmlEscape = (value) =>
  String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");

const toc = (entries) => {
  const cached = entries
    .map(({ title: entryTitle, level, page }) => {
      const indent = Math.max(0, level - 1) * 360;
      return `<w:p>
        <w:pPr>
          <w:pStyle w:val="TOC${level}"/>
          <w:tabs><w:tab w:val="right" w:leader="dot" w:pos="9000"/></w:tabs>
          <w:ind w:left="${indent}"/>
        </w:pPr>
        <w:r><w:t>${xmlEscape(entryTitle)}</w:t></w:r>
        <w:r><w:tab/></w:r>
        <w:r><w:t>${xmlEscape(page)}</w:t></w:r>
      </w:p>`;
    })
    .join("");

  return ImportedXmlComponent.fromXmlString(`<w:sdt xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
    <w:sdtPr><w:alias w:val="目录"/></w:sdtPr>
    <w:sdtContent>
      <w:p>
        <w:r>
          <w:fldChar w:fldCharType="begin" w:dirty="true"/>
          <w:instrText xml:space="preserve"> TOC \\o &quot;1-3&quot; \\h \\z \\u </w:instrText>
          <w:fldChar w:fldCharType="separate"/>
        </w:r>
      </w:p>
      ${cached}
      <w:p><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>
    </w:sdtContent>
  </w:sdt>`).root[0];
};

// === 物料采购计划表格 ===
const purchaseTable = () => {
  const widths = [1400, 3200, 800, 800, 1400, 1600];
  const headerCell = (text) =>
    cell(text, {
      shading: { type: ShadingType.CLEAR, fill: palette.fill },
      width: { size: widths[0], type: WidthType.DXA },
    });
  const headerCell2 = (text, idx) =>
    cell(text, {
      shading: { type: ShadingType.CLEAR, fill: palette.fill },
      width: { size: widths[idx], type: WidthType.DXA },
    });

  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    columnWidths: widths,
    rows: [
      new TableRow({
        children: [
          headerCell("类别"),
          headerCell2("器件", 1),
          headerCell2("数量", 2),
          headerCell2("单价(元)", 3),
          headerCell2("采购渠道", 4),
          headerCell2("用途", 5),
        ],
      }),
      new TableRow({ children: [
        cell("核心器件"),
        cell("1.3寸 OLED显示屏（SSD1306，I2C，4针）"),
        cell("2"), cell("~21"), cell("淘宝"), cell("时间/传感器显示"),
      ]}),
      new TableRow({ children: [
        cell("核心器件"),
        cell("GY-521 MPU6050 模块（I2C，6轴）"),
        cell("2"), cell("~13"), cell("淘宝"), cell("姿态/计步传感器"),
      ]}),
      new TableRow({ children: [
        cell("核心器件"),
        cell("TP4056 Type-C 锂电池充电模块"),
        cell("1"), cell("~4.5"), cell("淘宝"), cell("锂电池充电管理"),
      ]}),
      new TableRow({ children: [
        cell("核心器件"),
        cell("3.7V 锂聚合物电池（602040，400mAh）"),
        cell("2"), cell("~5"), cell("淘宝"), cell("手表电源"),
      ]}),
      new TableRow({ children: [
        cell("扩展器件"),
        cell("EC11 旋转编码器（带开关，梅花柄15mm）"),
        cell("2"), cell("~2.8"), cell("淘宝"), cell("菜单切换/确认"),
      ]}),
      new TableRow({ children: [
        cell("扩展器件"),
        cell("HC-05 蓝牙串口模块（带底板，6针）"),
        cell("2"), cell("~14"), cell("淘宝"), cell("手机数据同步（仅安卓）"),
      ]}),
      new TableRow({ children: [
        cell("耗材/工具"),
        cell("1/4W 直插金属膜电阻包（30种/600个）"),
        cell("1"), cell("~9"), cell("淘宝"), cell("I2C上拉/限流/分压"),
      ]}),
      new TableRow({ children: [
        cell("耗材/工具"),
        cell("瓷片电容 104（100nF，100个/包）"),
        cell("1"), cell("~2"), cell("淘宝"), cell("去耦/滤波"),
      ]}),
      new TableRow({ children: [
        cell("耗材/工具"),
        cell("独石电容 106（10uF，5个/包）"),
        cell("3"), cell("~2.2"), cell("淘宝"), cell("滤波/储能"),
      ]}),
      new TableRow({ children: [
        cell("耗材/工具"),
        cell("6×6×5mm 直插轻触按键"),
        cell("1"), cell("~2.2"), cell("淘宝"), cell("开机/功能按键"),
      ]}),
      new TableRow({ children: [
        cell("耗材/工具"),
        cell("1N5819 肖特基二极管（直插，50只）"),
        cell("1"), cell("~4.2"), cell("淘宝"), cell("电池防反接保护"),
      ]}),
      new TableRow({ children: [
        cell("耗材/工具"),
        cell("CH340C USB转TTL 串口模块"),
        cell("1"), cell("~7.8"), cell("淘宝"), cell("串口调试日志打印"),
      ]}),
      new TableRow({ children: [
        cell("耗材/工具"),
        cell("5×7cm 洞洞板（绿油单面，2张/包）"),
        cell("2"), cell("~2"), cell("淘宝"), cell("焊接成品电路"),
      ]}),
      new TableRow({ children: [
        cell("已有物料"),
        cell("STM32F103C8T6 最小系统板"),
        cell("1"), cell("—"), cell("—"), cell("主控MCU"),
      ]}),
      new TableRow({ children: [
        cell("已有物料"),
        cell("ST-Link V2 调试器"),
        cell("1"), cell("—"), cell("—"), cell("程序烧录"),
      ]}),
      new TableRow({ children: [
        cell("已有物料"),
        cell("示波器、万用表、面包板、跳线等"),
        cell("1"), cell("—"), cell("—"), cell("调试/搭建"),
      ]}),
      new TableRow({ children: [
        cell("已有物料"),
        cell("18650 锂电池、电烙铁、锡膏、无铅焊锡"),
        cell("1"), cell("—"), cell("—"), cell("电源/焊接工具"),
      ]}),
    ],
  });
};

const sections = [
  {
    title: T`一、项目概述`,
    level: 1,
    page: 3,
    paragraphs: [
      T`本项目为《2026嵌入式系统课程设计》第一类题目（题目2）：基于STM32F103C8T6的智能手表设计。项目采用STM32F103C8T6作为主控MCU，利用FreeRTOS实时操作系统实现多任务管理，通过I2C总线连接1.3寸OLED显示屏和MPU6050陀螺仪/加速度计，实现时间显示、传感器数据采集与界面显示。电源部分采用3.7V锂电池供电，经LDO稳压至3.3V，并配备TP4056充电管理电路。`,
      T`项目目标：完成智能手表的硬件电路设计与软件系统开发，实现时间显示、传感器数据读取、OLED界面显示等基本功能；在此基础上，扩展旋转编码器多级菜单、蓝牙与手机数据同步等高级功能，争取达到“良好”及以上等级。`,
    ],
  },
  {
    title: T`二、工作计划`,
    level: 1,
    page: 4,
    paragraphs: [
      T`项目从选题到最终验收，共约12天，划分为以下阶段：`,
    ],
    table: () => {
      const widths = [2200, 4200, 2200];
      const h = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[0], type: WidthType.DXA } });
      const h2 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[1], type: WidthType.DXA } });
      const h3 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[2], type: WidthType.DXA } });
      return new Table({
        width: { size: 100, type: WidthType.PERCENTAGE },
        columnWidths: widths,
        rows: [
          new TableRow({ children: [h("阶段"), h2("主要工作内容"), h3("截止时间/里程碑")] }),
          new TableRow({ children: [cell("阶段1：选题与计划"), cell("完成选题，编写项目计划书"), cell("2026-06-30 9:00")] }),
          new TableRow({ children: [cell("阶段2：硬件采购与搭建"), cell("完成所有物料采购，搭建面包板原型电路"), cell("2026-07-01")] }),
          new TableRow({ children: [cell("阶段3：系统设计"), cell("编写系统设计文档，完成系统架构与详细设计"), cell("2026-07-02 9:00")] }),
          new TableRow({ children: [cell("阶段4：软件开发"), cell("移植FreeRTOS，编写I2C/OLED/MPU6050驱动，实现时间显示与传感器读取"), cell("2026-07-05")] }),
          new TableRow({ children: [cell("阶段5：扩展功能开发"), cell("实现旋转编码器菜单、蓝牙通信、计步算法"), cell("2026-07-07")] }),
          new TableRow({ children: [cell("阶段6：硬件焊接与集成"), cell("洞洞板焊接，整合外壳，完成系统联调"), cell("2026-07-08")] }),
          new TableRow({ children: [cell("阶段7：测试与验收"), cell("功能测试、性能优化，准备第一次验收"), cell("2026-07-09 9:00")] }),
          new TableRow({ children: [cell("阶段8：改进与最终验收"), cell("根据第一次验收意见修改，完成最终验收"), cell("2026-07-10 9:00")] }),
        ],
      });
    },
  },
  {
    title: T`三、时间安排与分工`,
    level: 1,
    page: 5,
    paragraphs: [
      T`项目组成员投入时间及分工协调如下：`,
    ],
    table: () => {
      const widths = [1600, 2000, 2000, 3000];
      const h = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[0], type: WidthType.DXA } });
      const h2 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[1], type: WidthType.DXA } });
      const h3 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[2], type: WidthType.DXA } });
      const h4 = (text) => cell(text, { shading: { type: ShadingType.CLEAR, fill: palette.fill }, width: { size: widths[3], type: WidthType.DXA } });
      return new Table({
        width: { size: 100, type: WidthType.PERCENTAGE },
        columnWidths: widths,
        rows: [
          new TableRow({ children: [h("成员"), h2("可投入时间"), h3("负责模块"), h4("具体职责")] }),
          new TableRow({ children: [cell("成员A"), cell("全天（约8h/天）"), cell("硬件设计"), cell("电路设计、物料采购、洞洞板焊接、电源调试")] }),
          new TableRow({ children: [cell("成员B"), cell("全天（约8h/天）"), cell("软件开发"), cell("FreeRTOS移植、驱动开发、传感器算法、界面逻辑")] }),
          new TableRow({ children: [cell("成员C（如有）"), cell("半天（约4h/天）"), cell("测试与文档"), cell("功能测试、蓝牙上位机设计、报告撰写")] }),
        ],
      });
    },
  },
  {
    title: T`四、物料采购计划`,
    level: 1,
    page: 6,
    paragraphs: [
      T`根据题目要求及现有物料情况，尚需采购以下器件。本组已拥有电脑、STM32F103C8T6最小系统板、ST-Link调试器、示波器、万用表、面包板、跳线（公对母/母对母/公对公）、18650锂电池、电烙铁、锡膏、锡浆、无铅焊锡等基础设备。`,
      T`需采购物料总成本预估约100~150元。其中核心器件约60~80元，扩展器件约30~40元，耗材约30~40元。所有器件均为直插或成品模块，面包板可直接搭建调试，无需额外设计PCB。建议统一在淘宝下单，预计2~3天内到货。`,
    ],
    table: purchaseTable,
  },
  {
    title: T`五、风险与应对措施`,
    level: 1,
    page: 8,
    paragraphs: [
      T`1. 物料延迟风险：部分模块（如OLED、MPU6050）若采购延迟，将直接影响硬件搭建进度。应对措施：优先在本地电子市场或京东自营购买，预留1天缓冲时间。`,
      T`2. 技术难点风险：FreeRTOS多任务调度可能出现死锁或优先级反转；MPU6050计步算法需要调试。应对措施：先实现单任务驱动，再逐步集成；计步算法先采用简单阈值法，后续优化。`,
      T`3. 蓝牙兼容性问题：HC-05为经典蓝牙2.0，仅支持安卓手机连接，不支持iPhone及Windows电脑蓝牙。应对措施：提前准备安卓测试手机，并了解蓝牙模块AT指令配置。`,
      T`4. 电池续航问题：18650电池体积较大，不适合手表佩戴；若使用602040小电池，容量有限。应对措施：主用602040锂聚合物电池作为演示电源，18650作为备用调试电源。`,
    ],
  },
];

const children = [
  para(run(title, { bold: true, size: 36, color: palette.dark }), {
    heading: HeadingLevel.TITLE,
    alignment: AlignmentType.CENTER,
    spacing: { after: 360 },
  }),
  para(run("目录", { bold: true, size: 30, color: palette.dark }), {
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 240, after: 120 },
  }),
  para(run(T`右键目录，选择"更新域"刷新页码。`, { italics: true, color: palette.light })),
  toc(
    sections.map(({ title: entryTitle, level, page }) => ({
      title: entryTitle,
      level,
      page,
    })),
  ),
];

for (const section of sections) {
  children.push(heading(section.title, section.level));
  for (const paragraph of section.paragraphs) {
    children.push(bodyPara(paragraph));
  }
  if (section.table) {
    children.push(section.table());
  }
}

const doc = new Document({
  features: { updateFields: true },
  sections: [
    {
      properties: {
        page: {
          margin: { top: 1440, bottom: 1440, left: 1440, right: 1440 },
        },
      },
      headers: {
        default: new Header({
          children: [
            para(run(title, { bold: true, color: palette.primary }), {
              alignment: AlignmentType.CENTER,
            }),
          ],
        }),
      },
      footers: {
        default: new Footer({
          children: [
            para(new TextRun({ children: [PageNumber.CURRENT] }), {
              alignment: AlignmentType.CENTER,
            }),
          ],
        }),
      },
      children,
    },
  ],
});

const buffer = await Packer.toBuffer(doc);
fs.writeFileSync(outputPath, buffer);
