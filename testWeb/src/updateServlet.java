import Thymeleaf.viewBaseServlet;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.io.BufferedReader;
import java.io.IOException;
import java.net.URLDecoder;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

@WebServlet("/update")
public class updateServlet extends viewBaseServlet {

    @Override
    protected void doPut(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        req.setCharacterEncoding("UTF-8");
        resp.setContentType("text/html;charset=UTF-8");

        // 读取并解析请求体
        BufferedReader reader = req.getReader();
        StringBuilder payload = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) {
            payload.append(line);
        }

        String body = new BufferedReader(req.getReader()).lines().collect(Collectors.joining());
        String[] pairs = body.split("&");
        String oldNote = null, newNote = null;

        for (String pair : pairs) {
            String[] kv = pair.split("=");
            if (kv.length == 2) {
                if (kv[0].equals("old")) oldNote = URLDecoder.decode(kv[1], "UTF-8");
                if (kv[0].equals("note")) newNote = URLDecoder.decode(kv[1], "UTF-8");
            }
        }


        System.out.println("🔁 修改内容：old=" + oldNote + " → new=" + newNote);

        // 更新 session 中的内容
        HttpSession session = req.getSession();
        List<String> allContents = (List<String>) session.getAttribute("allContents");
        if (allContents == null) {
            allContents = new ArrayList<>();
        }

        if (oldNote != null && newNote != null) {
            int index = allContents.indexOf(oldNote);
            if (index != -1) {
                allContents.set(index, newNote);
            }
            session.setAttribute("allContents", allContents);
        }

        req.setAttribute("allContents", allContents);

        // 返回页面
        super.processTemplate("index", req, resp);
    }

}
